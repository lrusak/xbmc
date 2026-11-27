/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkLinux.h"

#include "network/NetworkUtils.h"
#include "utils/FileHandle.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <errno.h>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <thread>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ip_icmp.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace KODI;
using namespace KODI::UTILS::POSIX;

namespace
{

constexpr unsigned int ICMP_PACKET_SIZE{64};
constexpr unsigned int TTL{64};

bool PingWithCommand(unsigned long remoteIp, unsigned int timeoutMs)
{
  // A zero timeout is a nonblocking probe; a new child cannot provide a result yet.
  if (timeoutMs == 0)
    return false;

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  in_addr address{};
  address.s_addr = remoteIp;
  char host[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &address, host, sizeof(host)))
    return false;

  const pid_t child = fork();
  if (child < 0)
  {
    CLog::Log(LOGERROR, "ping fork failed: {} ({})", strerror(errno), errno);
    return false;
  }
  if (child == 0)
  {
    const int nullFd = open("/dev/null", O_RDWR);
    if (nullFd < 0 || dup2(nullFd, STDIN_FILENO) < 0 || dup2(nullFd, STDOUT_FILENO) < 0 ||
        dup2(nullFd, STDERR_FILENO) < 0)
      _exit(127);
    if (nullFd > STDERR_FILENO)
      close(nullFd);

    // Avoid ping-specific timeout options; their syntax and resolution vary by implementation.
    execlp("ping", "ping", "-n", "-c", "1", host, static_cast<char*>(nullptr));
    _exit(127);
  }

  int status;
  while (true)
  {
    const pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child)
    {
      if (!WIFEXITED(status) || WEXITSTATUS(status) > 1)
        CLog::Log(LOGERROR, "ping failed for {}: status={}", host, status);
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    if (result < 0 && errno != EINTR)
    {
      CLog::Log(LOGERROR, "ping waitpid failed: {} ({})", strerror(errno), errno);
      return false;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline)
      break;
    std::this_thread::sleep_until(std::min(deadline, now + std::chrono::milliseconds(10)));
  }

  // Kodi owns the timeout, so terminate and reap ping if it outlives the deadline.
  kill(child, SIGKILL);
  while (waitpid(child, &status, 0) < 0 && errno == EINTR)
  {
  }
  return false;
}

struct IcmpPacket
{
  icmphdr header;
  uint8_t data[ICMP_PACKET_SIZE - sizeof(icmphdr)];

  uint16_t Checksum()
  {
    auto data = reinterpret_cast<const uint16_t*>(this);
    unsigned int length = sizeof(IcmpPacket);

    unsigned int sum;

    for (sum = 0; length > 1; length -= 2)
    {
      sum += *data++;
    }

    if (length == 1)
    {
      sum += *data;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);

    sum += (sum >> 16);

    return ~sum;
  }
};

} // namespace

CNetworkInterfaceLinux::CNetworkInterfaceLinux(CNetworkPosix* network,
                                               std::string interfaceName,
                                               char interfaceMacAddrRaw[6])
  : CNetworkInterfacePosix(network, std::move(interfaceName), interfaceMacAddrRaw)
{
}

std::string CNetworkInterfaceLinux::GetCurrentDefaultGateway() const
{
  std::ifstream routes("/proc/net/route");
  std::string line;

  // skip the header
  std::getline(routes, line);

  while (std::getline(routes, line))
  {
    std::istringstream fields(line);
    std::string iface;
    std::string destination;
    std::string gateway;
    if (!(fields >> iface >> destination >> gateway))
      continue;

    if (iface != m_interfaceName || destination != "00000000" || gateway == "00000000")
      continue;

    // The kernel prints the address as a hex number of its network byte order value
    in_addr address{};
    const char* const last = gateway.data() + gateway.size();
    const auto [end, error] = std::from_chars(gateway.data(), last, address.s_addr, 16);
    if (error == std::errc{} && end == last)
      return NETWORK::GetIpStr(address);
  }

  return {};
}

bool CNetworkInterfaceLinux::GetHostMacAddress(unsigned long host_ip, std::string& mac) const
{
  struct arpreq areq;
  struct sockaddr_in* sin;

  memset(&areq, 0x0, sizeof(areq));

  sin = (struct sockaddr_in*)&areq.arp_pa;
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = host_ip;

  sin = (struct sockaddr_in*)&areq.arp_ha;
  sin->sin_family = ARPHRD_ETHER;

  strncpy(areq.arp_dev, m_interfaceName.c_str(), sizeof(areq.arp_dev));
  areq.arp_dev[sizeof(areq.arp_dev) - 1] = '\0';

  int result = ioctl(m_network->GetSocket(), SIOCGARP, (caddr_t)&areq);

  if (result != 0)
  {
    //  CLog::Log(LOGERROR, "{} - GetHostMacAddress/ioctl failed with errno ({})", __FUNCTION__, errno);
    return false;
  }

  struct sockaddr* res = &areq.arp_ha;
  mac = StringUtils::Format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", (uint8_t)res->sa_data[0],
                            (uint8_t)res->sa_data[1], (uint8_t)res->sa_data[2],
                            (uint8_t)res->sa_data[3], (uint8_t)res->sa_data[4],
                            (uint8_t)res->sa_data[5]);

  for (int i = 0; i < 6; ++i)
    if (res->sa_data[i])
      return true;

  return false;
}

std::unique_ptr<CNetworkBase> CNetworkBase::GetNetwork()
{
  return std::make_unique<CNetworkLinux>();
}

CNetworkLinux::CNetworkLinux() : CNetworkPosix()
{
  queryInterfaceList();
}

void CNetworkLinux::GetMacAddress(const std::string& interfaceName, char rawMac[6])
{
  memset(rawMac, 0, 6);

  ifreq ifr{};
  if (NETWORK::SetInterfaceName(ifr, interfaceName) && ioctl(GetSocket(), SIOCGIFHWADDR, &ifr) >= 0)
  {
    memcpy(rawMac, ifr.ifr_hwaddr.sa_data, 6);
  }
}

void CNetworkLinux::queryInterfaceList()
{
  std::ifstream devices("/proc/net/dev");
  if (!devices)
    return;

  CNetworkInterfaces::InterfaceList interfaces;
  std::string line;

  // skip the two header lines
  for (int i = 0; i < 2 && std::getline(devices, line); ++i)
  {
  }

  while (std::getline(devices, line))
  {
    // Each line starts with the interface name followed by ':'
    const std::size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;

    const std::string interfaceName = line.substr(start, line.find_first_of(": \t", start) - start);

    char macAddrRaw[6];
    GetMacAddress(interfaceName, macAddrRaw);

    // only add interfaces with non-zero mac addresses
    if (std::ranges::any_of(macAddrRaw, [](char byte) { return byte != 0; }))
      interfaces.push_back(
          std::make_unique<CNetworkInterfaceLinux>(this, interfaceName, macAddrRaw));
  }

  GetInterfaces().SetInterfaces(std::move(interfaces));
}

std::vector<std::string> CNetworkLinux::GetNameServers()
{
  std::vector<std::string> result;

  res_init();

  for (int i = 0; i < _res.nscount; i++)
  {
    result.push_back(NETWORK::GetIpStr(_res.nsaddr_list[i].sin_addr));
  }
  return result;
}

bool CNetworkLinux::IcmpPing(unsigned long remote_ip, const std::chrono::milliseconds timeout)
{
  CFileHandle fd(socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_ICMP));
  if (!fd)
  {
    if (errno == EACCES || errno == EPERM)
    {
      CLog::Log(LOGDEBUG, "ICMP socket permission denied, falling back to ping");
      return PingWithCommand(remote_ip, static_cast<unsigned int>(timeout.count()));
    }
    CLog::Log(LOGERROR, "socket failed: {} ({})", strerror(errno), errno);
    return false;
  }

  int ret = setsockopt(fd, SOL_IP, IP_TTL, &TTL, sizeof(TTL));
  if (ret != 0)
  {
    CLog::Log(LOGERROR, "setsockopt failed: {} ({})", strerror(errno), errno);
    return false;
  }

  CFileHandle epfd(epoll_create1(EPOLL_CLOEXEC));
  if (!epfd)
  {
    CLog::Log(LOGERROR, "epoll_create1 failed: {} ({})", strerror(errno), errno);
    return false;
  }

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = fd;
  ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "epoll_ctl failed: {} ({})", strerror(errno), errno);
    return false;
  }

  IcmpPacket packet = {};

  packet.header.type = ICMP_ECHO;
  packet.header.un.echo.id = getpid();

  packet.header.un.echo.sequence = 0;
  packet.header.checksum = packet.Checksum();

  sockaddr_in addr = {};
  addr.sin_addr.s_addr = remote_ip;
  addr.sin_family = AF_INET;

  const auto start = std::chrono::steady_clock::now();

  ret = sendto(fd, &packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (ret < 1)
  {
    CLog::Log(LOGERROR, "sendto failed: {} ({})", strerror(errno), errno);
    return false;
  }

  event = {};
  ret = epoll_wait(epfd, &event, 1, timeout.count());
  if (ret < 1)
  {
    if (ret == 0)
    {
      CLog::Log(LOGERROR, "timed out while waiting to receive ({} ms)", timeout.count());
    }
    else
    {
      CLog::Log(LOGERROR, "epoll_wait failed: {} ({})", strerror(errno), errno);
    }

    return false;
  }

  if (event.events & EPOLLIN)
  {
    socklen_t length = sizeof(addr);
    ret = recvfrom(fd, &packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&addr), &length);
    if (ret < 1)
    {
      CLog::Log(LOGERROR, "recvfrom failed: {} ({})", strerror(errno), errno);
      return false;
    }
  }

  const auto end = std::chrono::steady_clock::now();

  if (!(packet.header.type == ICMP_ECHOREPLY && packet.header.code == 0))
  {
    CLog::Log(LOGERROR, "unexpected ping reply: type={} code={}", packet.header.type,
              packet.header.code);
    return false;
  }
  else
  {
    const auto duration =
        std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(end - start);

    CLog::Log(LOGDEBUG, "PING {}: icmp_seq={} ttl={} time={:0.3f} ms",
              NETWORK::GetIpStr(addr.sin_addr), packet.header.un.echo.sequence, TTL,
              duration.count());
    return true;
  }
}
