/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "network/NetworkServices/INetworkService.h"
#include "settings/lib/ISettingCallback.h"

class CNetworkServices;

class CWebServer;

class CSettings;

class CHTTPImageHandler;
class CHTTPImageTransformationHandler;
class CHTTPVfsHandler;
class CHTTPJsonRpcHandler;
#ifdef HAS_PYTHON
class CHTTPPythonHandler;
#endif
class CHTTPWebinterfaceHandler;
class CHTTPWebinterfaceAddonsHandler;

class CWebServerService : public INetworkService, public ISettingCallback
{
public:
  CWebServerService();
  virtual ~CWebServerService() override;

  static void Register(CNetworkServices* networkServices);

  // ISettingCallback overrides
  bool OnSettingChanging(const std::shared_ptr<const CSetting>& setting);
  bool OnSettingUpdate(const std::shared_ptr<CSetting>& setting,
                       const char* oldSettingId,
                       const TiXmlNode* oldSettingNode);

  // INetworkService overrides
  std::string Name() override { return "webserver"; }
  void Start() override;
  void Stop(bool wait) override;

private:
  bool StartWebserver();
  bool IsWebserverRunning();
  bool StopWebserver();

  std::unique_ptr<CWebServer> m_webserver;

  std::shared_ptr<CSettings> m_settings;

  std::unique_ptr<CHTTPImageHandler> m_httpImageHandler;
  std::unique_ptr<CHTTPImageTransformationHandler> m_httpImageTransformationHandler;
  std::unique_ptr<CHTTPVfsHandler> m_httpVfsHandler;
  std::unique_ptr<CHTTPJsonRpcHandler> m_httpJsonRpcHandler;
#ifdef HAS_PYTHON
  std::unique_ptr<CHTTPPythonHandler> m_httpPythonHandler;
#endif
  std::unique_ptr<CHTTPWebinterfaceHandler> m_httpWebinterfaceHandler;
  std::unique_ptr<CHTTPWebinterfaceAddonsHandler> m_httpWebinterfaceAddonsHandler;
};
