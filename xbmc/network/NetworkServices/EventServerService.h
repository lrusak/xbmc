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

class CEventServerService : public INetworkService, public ISettingCallback
{
public:
  CEventServerService();
  virtual ~CEventServerService() override = default;

  static void Register(CNetworkServices* networkServices);

  // ISettingCallback overrides
  bool OnSettingChanging(const std::shared_ptr<const CSetting>& setting);

  // INetworkService overrides
  std::string Name() override { return "eventserver"; }
  void Start() override;
  void Stop(bool wait) override;

private:
  bool StartEventServer();
  bool IsEventServerRunning();
  bool StopEventServer(bool wait, bool promptuser);
  bool RefreshEventServer();

  std::shared_ptr<CSettings> m_settings;
};
