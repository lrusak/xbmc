/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkPosix.h"

#include "network/NetworkUtils.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <cstring>
#include <utility>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

using namespace KODI;

bool NETWORK::SetInterfaceName(ifreq& request, std::string_view name)
{
  if (name.size() >= sizeof(request.ifr_name))
    return false;

  std::memcpy(request.ifr_name, name.data(), name.size());
  request.ifr_name[name.size()] = '\0';
  return true;
}

CNetworkInterfacePosix::CNetworkInterfacePosix(CNetworkPosix* network,
                                               std::string interfaceName,
                                               const NETWORK::MacAddress& interfaceMacAddrRaw)
  : m_interfaceName(std::move(interfaceName)),
    m_network(network),
    m_interfaceMacAdr(NETWORK::FormatMacAddress(interfaceMacAddrRaw)),
    m_interfaceMacAddrRaw(interfaceMacAddrRaw)
{
}

bool CNetworkInterfacePosix::IsEnabled() const
{
  ifreq ifr{};
  if (!NETWORK::SetInterfaceName(ifr, m_interfaceName) ||
      ioctl(m_network->GetSocket(), SIOCGIFFLAGS, &ifr) < 0)
    return false;

  return ((ifr.ifr_flags & IFF_UP) == IFF_UP);
}

bool CNetworkInterfacePosix::IsConnected() const
{
  ifreq ifr{};
  if (!NETWORK::SetInterfaceName(ifr, m_interfaceName) ||
      ioctl(m_network->GetSocket(), SIOCGIFFLAGS, &ifr) < 0)
    return false;

  // ignore loopback
  if (!(ifr.ifr_flags & IFF_RUNNING) || (ifr.ifr_flags & IFF_LOOPBACK))
    return false;

  if (ioctl(m_network->GetSocket(), SIOCGIFADDR, &ifr) < 0)
    return false;

  // return only interfaces which has ip address
  return reinterpret_cast<const sockaddr_in*>(&ifr.ifr_addr)->sin_addr.s_addr != INADDR_ANY;
}

std::string CNetworkInterfacePosix::GetCurrentIPAddress() const
{
  std::string result;

  ifreq ifr{};
  ifr.ifr_addr.sa_family = AF_INET;
  if (NETWORK::SetInterfaceName(ifr, m_interfaceName) &&
      ioctl(m_network->GetSocket(), SIOCGIFADDR, &ifr) >= 0)
  {
    result = NETWORK::GetIpStr(reinterpret_cast<const sockaddr_in*>(&ifr.ifr_addr)->sin_addr);
  }

  return result;
}

std::string CNetworkInterfacePosix::GetCurrentNetmask() const
{
  std::string result;

  ifreq ifr{};
  ifr.ifr_addr.sa_family = AF_INET;
  if (NETWORK::SetInterfaceName(ifr, m_interfaceName) &&
      ioctl(m_network->GetSocket(), SIOCGIFNETMASK, &ifr) >= 0)
  {
    result = NETWORK::GetIpStr(reinterpret_cast<const sockaddr_in*>(&ifr.ifr_addr)->sin_addr);
  }

  return result;
}

std::string CNetworkInterfacePosix::GetMacAddress() const
{
  return m_interfaceMacAdr;
}

NETWORK::MacAddress CNetworkInterfacePosix::GetMacAddressRaw() const
{
  return m_interfaceMacAddrRaw;
}

CNetworkPosix::CNetworkPosix() : CNetworkBase()
{
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);
}

CNetworkPosix::~CNetworkPosix()
{
  if (m_sock != -1)
    close(CNetworkPosix::m_sock);
}

// Requery the interfaces while none is connected, so that an interface that only comes up after
// start is found.
void CNetworkPosix::UpdateInterfaces()
{
  if (!GetInterfaces().IsConnected())
  {
    CLog::Log(LOGDEBUG, "{} no connected interface found - requery list", __FUNCTION__);
    queryInterfaceList();
  }
}
