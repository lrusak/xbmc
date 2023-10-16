/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/lib/ISettingCallback.h"

#include <mutex>
#include <string>
#include <vector>

#include "PlatformDefs.h"

class CNetworkInterface
{
public:
   virtual ~CNetworkInterface() = default;

   virtual bool IsEnabled(void) const = 0;
   virtual bool IsConnected(void) const = 0;

   virtual std::string GetMacAddress(void) const = 0;
   virtual void GetMacAddressRaw(char rawMac[6]) const = 0;

   virtual bool GetHostMacAddress(unsigned long host, std::string& mac) const = 0;

   virtual std::string GetCurrentIPAddress() const = 0;
   virtual std::string GetCurrentNetmask() const = 0;
   virtual std::string GetCurrentDefaultGateway(void) const = 0;
};

class CSettings;
class CNetworkServices;

class CNetworkBase
{
public:
  enum EMESSAGE
  {
    SERVICES_UP,
    SERVICES_DOWN
  };

  static std::unique_ptr<CNetworkBase> GetNetwork();

  CNetworkBase();
  virtual ~CNetworkBase();

  // Get network services
  CNetworkServices& GetServices() { return *m_services; }

  // Return our hostname
  virtual bool GetHostName(std::string& hostname);

  // Return the list of interfaces
  virtual std::vector<CNetworkInterface*>& GetInterfaceList(void) = 0;

  // Hold this guard throughout access to GetInterfaceList(). Platforms can override
  // it to synchronize those reads with interface-list refreshes.
  [[nodiscard]] virtual std::unique_lock<std::mutex> LockInterfaceList() { return {}; }

  // Return the first interface which is active
  virtual CNetworkInterface* GetFirstConnectedInterface(void);

  // Return true if there is a interface for the same network as address
  bool HasInterfaceForIP(unsigned long address);

  // Return true if there's at least one defined network interface
  bool IsAvailable(void);

  // Return true if there's at least one interface which is connected
  bool IsConnected(void);

  // Return true if host replies to ping
  bool PingHost(unsigned long host,
                unsigned short port,
                unsigned int timeout_ms = 2000,
                bool readability_check = false);
  virtual bool IcmpPing(unsigned long host, unsigned int timeout_ms = 2000) = 0;

  // Get/set the nameserver(s)
  virtual std::vector<std::string> GetNameServers(void) = 0;

  // callback from application controlled thread to handle any setup
  void NetworkMessage(EMESSAGE message, int param);

  // Return true if given name or ip address corresponds to localhost
  bool IsLocalHost(const std::string& hostname);

  // Waits for the first network interface to become available
  void WaitForNet();

  std::unique_ptr<CNetworkServices> m_services;
};
