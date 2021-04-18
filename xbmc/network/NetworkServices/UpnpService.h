/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "network/NetworkServices/INetworkService.h"
#include "settings/lib/ISettingCallback.h"

class CNetworkServices;

class CSettings;

class CUpnpService : public INetworkService, public ISettingCallback
{
public:
  CUpnpService();
  virtual ~CUpnpService() override = default;

  static void Register(CNetworkServices* networkServices);

  // ISettingCallback overrides
  bool OnSettingChanging(const std::shared_ptr<const CSetting>& setting);

  // INetworkService overrides
  std::string Name() override { return "upnp"; }
  void Start() override;
  void Stop(bool wait) override;

private:
  bool StartUPnP();
  bool StopUPnP(bool wait);
  bool StartUPnPClient();
  bool IsUPnPClientRunning();
  bool StopUPnPClient();
  bool StartUPnPController();
  bool IsUPnPControllerRunning();
  bool StopUPnPController();
  bool StartUPnPRenderer();
  bool IsUPnPRendererRunning();
  bool StopUPnPRenderer();
  bool StartUPnPServer();
  bool IsUPnPServerRunning();
  bool StopUPnPServer();

  std::shared_ptr<CSettings> m_settings;
};
