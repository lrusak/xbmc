/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkPosix.h"

#include "utils/StringUtils.h"
#include "utils/log.h"

#include <utility>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

CNetworkInterfacePosix::CNetworkInterfacePosix(CNetworkPosix* network,
                                               std::string interfaceName,
                                               char interfaceMacAddrRaw[6])
  : m_interfaceName(std::move(interfaceName)),
    m_interfaceMacAdr(StringUtils::Format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                                          (uint8_t)interfaceMacAddrRaw[0],
                                          (uint8_t)interfaceMacAddrRaw[1],
                                          (uint8_t)interfaceMacAddrRaw[2],
                                          (uint8_t)interfaceMacAddrRaw[3],
                                          (uint8_t)interfaceMacAddrRaw[4],
                                          (uint8_t)interfaceMacAddrRaw[5]))
{
  m_network = network;
  memcpy(m_interfaceMacAddrRaw, interfaceMacAddrRaw, sizeof(m_interfaceMacAddrRaw));
}

bool CNetworkInterfacePosix::IsEnabled() const
{
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  if (ioctl(m_network->GetSocket(), SIOCGIFFLAGS, &ifr) < 0)
    return false;

  return ((ifr.ifr_flags & IFF_UP) == IFF_UP);
}

bool CNetworkInterfacePosix::IsConnected() const
{
  struct ifreq ifr;
  int zero = 0;
  memset(&ifr, 0, sizeof(struct ifreq));
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  if (ioctl(m_network->GetSocket(), SIOCGIFFLAGS, &ifr) < 0)
    return false;

  // ignore loopback
  int iRunning = ((ifr.ifr_flags & IFF_RUNNING) && (!(ifr.ifr_flags & IFF_LOOPBACK)));

  if (ioctl(m_network->GetSocket(), SIOCGIFADDR, &ifr) < 0)
    return false;

  // return only interfaces which has ip address
  return iRunning && (0 != memcmp(ifr.ifr_addr.sa_data + sizeof(short), &zero, sizeof(int)));
}

std::string CNetworkInterfacePosix::GetCurrentIPAddress() const
{
  std::string result;

  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  ifr.ifr_addr.sa_family = AF_INET;
  if (ioctl(m_network->GetSocket(), SIOCGIFADDR, &ifr) >= 0)
  {
    result = inet_ntoa((*((struct sockaddr_in*)&ifr.ifr_addr)).sin_addr);
  }

  return result;
}

std::string CNetworkInterfacePosix::GetCurrentNetmask() const
{
  std::string result;

  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  ifr.ifr_addr.sa_family = AF_INET;
  if (ioctl(m_network->GetSocket(), SIOCGIFNETMASK, &ifr) >= 0)
  {
    result = inet_ntoa((*((struct sockaddr_in*)&ifr.ifr_addr)).sin_addr);
  }

  return result;
}

std::string CNetworkInterfacePosix::GetMacAddress() const
{
  return m_interfaceMacAdr;
}

void CNetworkInterfacePosix::GetMacAddressRaw(char rawMac[6]) const
{
  memcpy(rawMac, m_interfaceMacAddrRaw, 6);
}

CNetworkPosix::CNetworkPosix() : CNetworkBase()
{
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);
}

CNetworkPosix::~CNetworkPosix()
{
  if (m_sock != -1)
    close(CNetworkPosix::m_sock);

  std::vector<CNetworkInterface*>::iterator it = m_interfaces.begin();
  while (it != m_interfaces.end())
  {
    CNetworkInterface* nInt = *it;
    delete nInt;
    it = m_interfaces.erase(it);
  }
}

std::vector<CNetworkInterface*>& CNetworkPosix::GetInterfaceList()
{
  return m_interfaces;
}

//! @bug
//! Overwrite the GetFirstConnectedInterface and requery
//! the interface list if no connected device is found
//! this fixes a bug when no network is available after first start of xbmc
//! and the interface comes up during runtime
CNetworkInterface* CNetworkPosix::GetFirstConnectedInterface()
{
  CNetworkInterface* pNetIf = CNetworkBase::GetFirstConnectedInterface();

  // no connected Interfaces found? - requeryInterfaceList
  if (!pNetIf)
  {
    CLog::Log(LOGDEBUG, "{} no connected interface found - requery list", __FUNCTION__);
    queryInterfaceList();
    //retry finding a connected if
    pNetIf = CNetworkBase::GetFirstConnectedInterface();
  }

  return pNetIf;
}

static const char* ConnectHostPort(SOCKET soc,
                                   const struct sockaddr_in& addr,
                                   struct timeval& timeOut,
                                   bool tryRead)
{
  // set non-blocking
  int result = fcntl(soc, F_SETFL, fcntl(soc, F_GETFL) | O_NONBLOCK);

  if (result != 0)
    return "set non-blocking option failed";

  result = connect(soc, (const struct sockaddr*)&addr,
                   sizeof(addr)); // non-blocking connect, will fail ..

  if (result < 0)
  {
    if (errno != EINPROGRESS)
      return "unexpected connect fail";

    { // wait for connect to complete
      fd_set wset;
      FD_ZERO(&wset);
      FD_SET(soc, &wset);

      result = select(FD_SETSIZE, 0, &wset, 0, &timeOut);
    }

    if (result < 0)
      return "select fail";

    if (result == 0) // timeout
      return ""; // no error

    { // verify socket connection state
      int err_code = -1;
      socklen_t code_len = sizeof(err_code);

      result = getsockopt(soc, SOL_SOCKET, SO_ERROR, (char*)&err_code, &code_len);

      if (result != 0)
        return "getsockopt fail";

      if (err_code != 0)
        return ""; // no error, just not connected
    }
  }

  if (tryRead)
  {
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(soc, &rset);

    result = select(FD_SETSIZE, &rset, 0, 0, &timeOut);

    if (result > 0)
    {
      char message[32];

      result = recv(soc, message, sizeof(message), 0);
    }

    if (result == 0)
      return ""; // no reply yet

    if (result < 0)
      return "recv fail";
  }

  return 0; // success
}

bool CNetworkPosix::PingHost(unsigned long ipaddr,
                             unsigned short port,
                             unsigned int timeOutMs,
                             bool readability_check)
{
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = ipaddr;

  SOCKET soc = socket(AF_INET, SOCK_STREAM, 0);

  const char* err_msg = "invalid socket";

  if (soc != INVALID_SOCKET)
  {
    struct timeval tmout;
    tmout.tv_sec = timeOutMs / 1000;
    tmout.tv_usec = (timeOutMs % 1000) * 1000;

    err_msg = ConnectHostPort(soc, addr, tmout, readability_check);

    (void)closesocket(soc);
  }

  if (err_msg && *err_msg)
  {
    std::string sock_err = strerror(errno);

    CLog::Log(LOGERROR, "{}({}:{}) - {} ({})", __FUNCTION__, inet_ntoa(addr.sin_addr), port,
              err_msg, sock_err);
  }

  return err_msg == 0;
}
