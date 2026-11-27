/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "network/Network.h"

#include <string>
#include <string_view>
#include <vector>

class CNetworkPosix;
struct ifreq;

namespace KODI::NETWORK
{
/*!
 * \brief Sets the interface name of an ioctl request.
 * \return false if the name doesn't fit
 */
bool SetInterfaceName(ifreq& request, std::string_view name);
} // namespace KODI::NETWORK

class CNetworkInterfacePosix : public CNetworkInterface
{
public:
  CNetworkInterfacePosix(CNetworkPosix* network,
                         std::string interfaceName,
                         char interfaceMacAddrRaw[6]);
  virtual ~CNetworkInterfacePosix() override = default;

  bool IsEnabled() const override;
  bool IsConnected() const override;
  std::string GetCurrentIPAddress() const override;
  std::string GetCurrentNetmask() const override;

  std::string GetMacAddress() const override;
  void GetMacAddressRaw(char rawMac[6]) const override;

protected:
  std::string m_interfaceName;
  CNetworkPosix* m_network;

private:
  std::string m_interfaceMacAdr;
  char m_interfaceMacAddrRaw[6];
};

class CNetworkPosix : public CNetworkBase
{
public:
  virtual ~CNetworkPosix() override;

  int GetSocket() { return m_sock; }

protected:
  CNetworkPosix();

  void UpdateInterfaces() override;

private:
  virtual void GetMacAddress(const std::string& interfaceName, char macAddrRaw[6]) = 0;
  virtual void queryInterfaceList() = 0;
  int m_sock;
};
