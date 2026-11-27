/*
 *  Copyright (C) 2016 Christian Browet
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "network/Network.h"

#include "platform/android/activity/JNIXBMCConnectivityManagerNetworkCallback.h"

#include <optional>

#include <androidjni/LinkProperties.h>
#include <androidjni/Network.h>
#include <androidjni/NetworkInterface.h>

class CNetworkInterfaceAndroid : public CNetworkInterface
{
public:
  CNetworkInterfaceAndroid(const CJNINetwork& network,
                           const CJNILinkProperties& lp,
                           const CJNINetworkInterface& intf);

  // CNetworkInterface interface
public:
  bool IsEnabled() const override;
  bool IsConnected() const override;
  std::string GetMacAddress() const override;
  KODI::NETWORK::MacAddress GetMacAddressRaw() const override;
  bool GetHostMacAddress(unsigned long host_ip, std::string& mac) const override;
  std::string GetCurrentIPAddress() const override;
  std::string GetCurrentNetmask() const override;
  std::string GetCurrentDefaultGateway() const override;

  std::string GetHostName() const override;
  std::vector<std::string> GetNameServers() const override;

protected:
  std::optional<KODI::NETWORK::MacAddress> GetHardwareAddress() const;

  std::string m_name;
  CJNINetwork m_network;
  CJNILinkProperties m_lp;
  CJNINetworkInterface m_intf;
};

class CNetworkAndroid : public CNetworkBase, public jni::CJNIXBMCConnectivityManagerNetworkCallback
{

public:
  CNetworkAndroid();
  ~CNetworkAndroid() override;

  // CNetwork interface
public:
  std::vector<std::string> GetNameServers() override;

  // Ping remote host
  using CNetworkBase::PingHost;
  bool IcmpPing(unsigned long remote_ip,
                const std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) override;

protected:
  void RetrieveInterfaces();

public:
  void onAvailable(const CJNINetwork network) override;
  void onLost(const CJNINetwork network) override;
};
