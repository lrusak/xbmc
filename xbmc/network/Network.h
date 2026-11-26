/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/lib/ISettingCallback.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
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

  virtual std::string GetHostName() const { return {}; }
  virtual std::vector<std::string> GetNameServers() const { return {}; }
};

/*!
 * \brief The interfaces of a network, which owns them.
 *
 * The list is replaced as a whole, so it can be requeried while other threads read it, and an
 * interface stays valid while a lookup still uses it. No pointer to an interface leaves this class.
 */
class CNetworkInterfaces
{
public:
  using InterfaceList = std::vector<std::unique_ptr<CNetworkInterface>>;

  /*!
   * \brief Replaces the interfaces, the preferred ones first.
   */
  void SetInterfaces(InterfaceList interfaces);

  /*!
   * \brief Sets the interface used for the values below whenever it is set, or clears it.
   */
  void SetDefaultInterface(std::unique_ptr<CNetworkInterface> interface);

  bool IsAvailable() const;
  bool IsConnected() const;

  //! The values of the default interface if set, else of the first connected interface,
  //! preferring one with a default gateway.
  //!@{
  std::string GetCurrentIPAddress() const;
  std::string GetMacAddress() const;
  bool GetMacAddressRaw(char rawMac[6]) const;
  std::string GetNetmask() const;
  std::string GetDefaultGateway() const;
  std::string GetHostName() const;
  std::vector<std::string> GetNameServers() const;
  //!@}

  //! \brief Whether an interface has the given address.
  bool HasIPAddress(std::string_view address) const;

  //! \brief Whether a connected interface is on the same subnet as the address.
  bool HasInterfaceForIP(unsigned long address) const;

  bool GetHostMacAddress(unsigned long host, std::string& mac) const;

private:
  struct State
  {
    std::shared_ptr<const InterfaceList> interfaces{std::make_shared<const InterfaceList>()};
    std::shared_ptr<const CNetworkInterface> defaultInterface;
  };

  //! Lookups query the interfaces outside the lock, as some queries block on the network.
  State GetState() const;
  static const CNetworkInterface* GetFirstConnectedInterface(const State& state);

  mutable std::mutex m_mutex;
  State m_state;
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

  // Values of the first connected interface, empty if there is none
  std::string GetCurrentIPAddress();
  std::string GetMacAddress();
  bool GetMacAddressRaw(char rawMac[6]);
  std::string GetNetmask();
  std::string GetDefaultGateway();

  // Return true if a connected interface knows the MAC address of the host
  bool GetHostMacAddress(unsigned long host, std::string& mac);

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
  virtual bool IcmpPing(
      unsigned long host,
      const std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) = 0;

  // Get/set the nameserver(s)
  virtual std::vector<std::string> GetNameServers(void) = 0;

  // callback from application controlled thread to handle any setup
  void NetworkMessage(EMESSAGE message, int param);

  // Return true if given name or ip address corresponds to localhost
  bool IsLocalHost(const std::string& hostname);

  // Waits for the first network interface to become available
  void WaitForNet();

  std::unique_ptr<CNetworkServices> m_services;

protected:
  /*!
   * \brief Runs before the interfaces are looked up.
   *
   * Platforms that are not told when interfaces change requery them here.
   */
  virtual void UpdateInterfaces() {}

  CNetworkInterfaces& GetInterfaces() { return m_interfaces; }

private:
  CNetworkInterfaces m_interfaces;
};
