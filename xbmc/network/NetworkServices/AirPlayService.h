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

class CAirPlayService : public INetworkService, public ISettingCallback
{
public:
  CAirPlayService();
  virtual ~CAirPlayService() override = default;

  static void Register(CNetworkServices* networkServices);

  // ISettingCallback overrides
  bool OnSettingChanging(const std::shared_ptr<const CSetting>& setting);

  // INetworkService overrides
  std::string Name() override { return "airtunes"; }
  void Start() override;
  void Stop(bool wait) override;

private:
  bool StartAirPlayServer();
  bool IsAirPlayServerRunning();
  bool StopAirPlayServer(bool bWait);
  bool StartAirTunesServer();
  bool IsAirTunesServerRunning();
  bool StopAirTunesServer(bool bWait);

  std::shared_ptr<CSettings> m_settings;
};
