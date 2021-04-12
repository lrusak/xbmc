/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/lib/ISettingCallback.h"

#include <memory>
#include <vector>

class INetworkService;
class CSettings;

class CNetworkServices : public ISettingCallback
{
public:
  CNetworkServices();
  ~CNetworkServices() override;

  void RegisterService(std::unique_ptr<INetworkService> service);

  bool OnSettingChanging(const std::shared_ptr<const CSetting>& setting) override;
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;

  void Start();
  void Stop(bool bWait);

  enum ESERVERS
  {
    ES_WEBSERVER = 1,
    ES_AIRPLAYSERVER,
    ES_JSONRPCSERVER,
    ES_UPNPRENDERER,
    ES_UPNPSERVER,
    ES_EVENTSERVER,
    ES_ZEROCONF
  };

  bool StartServer(enum ESERVERS server, bool start);

  bool StartAirPlayServer();
  bool IsAirPlayServerRunning();
  bool StopAirPlayServer(bool bWait);
  bool StartAirTunesServer();
  bool IsAirTunesServerRunning();
  bool StopAirTunesServer(bool bWait);

  bool StartJSONRPCServer();
  bool IsJSONRPCServerRunning();
  bool StopJSONRPCServer(bool bWait);

  bool StartEventServer();
  bool IsEventServerRunning();
  bool StopEventServer(bool bWait, bool promptuser);
  bool RefreshEventServer();

  bool StartUPnP();
  bool StopUPnP(bool bWait);
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

  bool StartRss();
  bool IsRssRunning();
  bool StopRss();

  bool StartZeroconf();
  bool IsZeroconfRunning();
  bool StopZeroconf();

  static bool ValidatePort(int port);

private:
  CNetworkServices(const CNetworkServices&);
  CNetworkServices const& operator=(CNetworkServices const&);

  // Construction parameters
  std::shared_ptr<CSettings> m_settings;

  std::vector<std::unique_ptr<INetworkService>> m_services;
};
