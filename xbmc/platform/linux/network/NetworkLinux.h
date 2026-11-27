/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "platform/posix/network/NetworkPosix.h"
#include "platform/posix/utils/FileHandle.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

class CNetworkInterfaceLinux : public CNetworkInterfacePosix
{
public:
  CNetworkInterfaceLinux(CNetworkPosix* network,
                         std::string interfaceName,
                         const KODI::NETWORK::MacAddress& interfaceMacAddrRaw);
  ~CNetworkInterfaceLinux() override = default;

  std::string GetCurrentDefaultGateway() const override;
  bool GetHostMacAddress(unsigned long host, std::string& mac) const override;
};

class CNetworkLinux : public CNetworkPosix
{
public:
  CNetworkLinux();
  ~CNetworkLinux() override;

  bool IcmpPing(unsigned long host,
                const std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) override;
  std::vector<std::string> GetNameServers(void) override;

protected:
  void UpdateInterfaces() override;

private:
  KODI::NETWORK::MacAddress GetMacAddress(const std::string& interfaceName) override;
  void queryInterfaceList() override;

  //! Requeries the interfaces whenever the kernel reports a change to links, addresses or routes.
  void WatchInterfaces();

  KODI::UTILS::POSIX::CFileHandle m_netlink;
  KODI::UTILS::POSIX::CFileHandle m_stopWatching;
  std::atomic_bool m_watching{false};
  std::thread m_watcher;
};
