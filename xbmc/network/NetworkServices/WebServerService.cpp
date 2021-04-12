/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "WebServerService.h"

#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "langinfo.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "network/Network.h"
#include "network/NetworkServices.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/SystemInfo.h"
#include "utils/log.h"

#ifdef HAS_ZEROCONF
#include "network/Zeroconf.h"
#endif

#include "network/WebServer.h"
#include "network/httprequesthandler/HTTPImageHandler.h"
#include "network/httprequesthandler/HTTPImageTransformationHandler.h"
#include "network/httprequesthandler/HTTPJsonRpcHandler.h"
#include "network/httprequesthandler/HTTPVfsHandler.h"
#ifdef HAS_PYTHON
#include "network/httprequesthandler/HTTPPythonHandler.h"
#endif
#include "network/httprequesthandler/HTTPWebinterfaceAddonsHandler.h"
#include "network/httprequesthandler/HTTPWebinterfaceHandler.h"

void CWebServerService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CWebServerService>());
}

CWebServerService::CWebServerService()
  : m_webserver(std::make_unique<CWebServer>()),
    m_httpImageHandler(std::make_unique<CHTTPImageHandler>()),
    m_httpImageTransformationHandler(std::make_unique<CHTTPImageTransformationHandler>()),
    m_httpVfsHandler(std::make_unique<CHTTPVfsHandler>()),
    m_httpJsonRpcHandler(std::make_unique<CHTTPJsonRpcHandler>()),
#ifdef HAS_PYTHON
    m_httpPythonHandler(std::make_unique<CHTTPPythonHandler>()),
#endif
    m_httpWebinterfaceHandler(std::make_unique<CHTTPWebinterfaceHandler>()),
    m_httpWebinterfaceAddonsHandler(std::make_unique<CHTTPWebinterfaceAddonsHandler>())
{
  m_webserver->RegisterRequestHandler(m_httpImageHandler.get());
  m_webserver->RegisterRequestHandler(m_httpImageTransformationHandler.get());
  m_webserver->RegisterRequestHandler(m_httpVfsHandler.get());
  m_webserver->RegisterRequestHandler(m_httpJsonRpcHandler.get());
#ifdef HAS_PYTHON
  m_webserver->RegisterRequestHandler(m_httpPythonHandler.get());
#endif
  m_webserver->RegisterRequestHandler(m_httpWebinterfaceAddonsHandler.get());
  m_webserver->RegisterRequestHandler(m_httpWebinterfaceHandler.get());

  std::set<std::string> settingSet{
      CWebServer::SETTING_SERVICES_WEBSERVER,
      CWebServer::SETTING_SERVICES_WEBSERVERPORT,
      CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION,
      CWebServer::SETTING_SERVICES_WEBSERVERUSERNAME,
      CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD,
      CWebServer::SETTING_SERVICES_WEBSERVERSSL,
  };

  auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (!settingsComponent)
    throw std::runtime_error("CWebServerService: CSettingsComponent isn't available!");

  m_settings = settingsComponent->GetSettings();
  if (!m_settings)
    throw std::runtime_error("CWebServerService: CSettings isn't available!");

  auto settingsManager = m_settings->GetSettingsManager();
  if (!settingsManager)
    throw std::runtime_error("CWebServerService: CSettingsManager isn't available!");

  settingsManager->RegisterCallback(this, settingSet);
}

CWebServerService::~CWebServerService()
{
  m_settings->GetSettingsManager()->UnregisterCallback(this);

  m_webserver->UnregisterRequestHandler(m_httpImageHandler.get());
  m_webserver->UnregisterRequestHandler(m_httpImageTransformationHandler.get());
  m_webserver->UnregisterRequestHandler(m_httpVfsHandler.get());
  m_webserver->UnregisterRequestHandler(m_httpJsonRpcHandler.get());

#ifdef HAS_PYTHON
  m_webserver->UnregisterRequestHandler(m_httpPythonHandler.get());
#endif
  m_webserver->UnregisterRequestHandler(m_httpWebinterfaceAddonsHandler.get());
  m_webserver->UnregisterRequestHandler(m_httpWebinterfaceHandler.get());
}

bool CWebServerService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();
  // Ask user to confirm disabling the authentication requirement, but not when the configuration
  // would be invalid when authentication was enabled (meaning that the change was triggered
  // automatically)
  if (settingId == CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION &&
      !std::static_pointer_cast<const CSettingBool>(setting)->GetValue() &&
      (!m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER) ||
       (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER) &&
        !m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD).empty())) &&
      KODI::MESSAGING::HELPERS::ShowYesNoDialogText(19098, 36634) !=
          KODI::MESSAGING::HELPERS::DialogResponse::YES)
  {
    // Leave it as-is
    return false;
  }

  if (settingId == CWebServer::SETTING_SERVICES_WEBSERVER ||
      settingId == CWebServer::SETTING_SERVICES_WEBSERVERPORT ||
      settingId == CWebServer::SETTING_SERVICES_WEBSERVERSSL ||
      settingId == CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION ||
      settingId == CWebServer::SETTING_SERVICES_WEBSERVERUSERNAME ||
      settingId == CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD)
  {
    if (m_webserver->IsStarted() && !StopWebserver())
      return false;

    if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER))
    {
      // Prevent changing to an invalid configuration
      if ((settingId == CWebServer::SETTING_SERVICES_WEBSERVER ||
           settingId == CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION ||
           settingId == CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD) &&
          m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION) &&
          m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD).empty())
      {
        if (settingId == CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION)
        {
          KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{257}, CVariant{36636});
        }
        else
        {
          KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{257}, CVariant{36635});
        }
        return false;
      }

      // Ask for confirmation when enabling the web server
      if (settingId == CWebServer::SETTING_SERVICES_WEBSERVER &&
          KODI::MESSAGING::HELPERS::ShowYesNoDialogText(19098, 36632) !=
              KODI::MESSAGING::HELPERS::DialogResponse::YES)
      {
        // Revert change, do not start server
        return false;
      }

      if (!StartWebserver())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33101}, CVariant{33100});
        return false;
      }
    }
  }
  else if (settingId == CWebServer::SETTING_SERVICES_WEBSERVERPORT)
    return CNetworkServices::ValidatePort(
        std::static_pointer_cast<const CSettingInt>(setting)->GetValue());

  return true;
}

bool CWebServerService::OnSettingUpdate(const std::shared_ptr<CSetting>& setting,
                                        const char* oldSettingId,
                                        const TiXmlNode* oldSettingNode)
{
  if (setting == NULL)
    return false;

  const std::string& settingId = setting->GetId();
  if (settingId == CWebServer::SETTING_SERVICES_WEBSERVERUSERNAME)
  {
    // if webserverusername is xbmc and pw is not empty we treat it as altered
    // and don't change the username to kodi - part of rebrand
    if (m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERUSERNAME) == "xbmc" &&
        !m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD).empty())
      return true;
  }
  if (settingId == CWebServer::SETTING_SERVICES_WEBSERVERPORT)
  {
    // if webserverport is default but webserver is activated then treat it as altered
    // and don't change the port to new value
    if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER))
      return true;
  }

  return false;
}

void CWebServerService::Start()
{
  // Start web server after eventserver and JSON-RPC server, so users can use these interfaces
  // to confirm the warning message below if it is shown
  if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER))
  {
    // services.webserverauthentication setting was added in Kodi v18 and requires a valid password
    // to be set, but on upgrade the setting will be activated automatically regardless of whether
    // a password was set before -> this can lead to an invalid configuration
    if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION) &&
        m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD).empty())
    {
      // Alert user to new default security settings in new Kodi version
      KODI::MESSAGING::HELPERS::ShowOKDialogText(33101, 33104);
      // Fix settings: Disable web server
      m_settings->SetBool(CWebServer::SETTING_SERVICES_WEBSERVER, false);
      // Bring user to settings screen where authentication can be configured properly
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(
          WINDOW_SETTINGS_SERVICE, std::vector<std::string>{"services.webserverauthentication"});
    }
    // Only try to start server if configuration is OK
    else if (!StartWebserver())
      CGUIDialogKaiToast::QueueNotification(
          CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33101), g_localizeStrings.Get(33100));
  }
}

void CWebServerService::Stop(bool wait)
{
  if (wait)
    StopWebserver();
}

bool CWebServerService::StartWebserver()
{
  if (!CServiceBroker::GetNetwork().IsAvailable())
    return false;

  if (!m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVER))
    return false;

  if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION) &&
      m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD).empty())
  {
    CLog::Log(LOGERROR, "Tried to start webserver with invalid configuration (authentication "
                        "enabled, but no password set");
    return false;
  }

  int webPort = m_settings->GetInt(CWebServer::SETTING_SERVICES_WEBSERVERPORT);
  if (!CNetworkServices::ValidatePort(webPort))
  {
    CLog::Log(LOGERROR, "Cannot start Web Server on port %i", webPort);
    return false;
  }

  if (IsWebserverRunning())
    return true;

  std::string username;
  std::string password;
  if (m_settings->GetBool(CWebServer::SETTING_SERVICES_WEBSERVERAUTHENTICATION))
  {
    username = m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERUSERNAME);
    password = m_settings->GetString(CWebServer::SETTING_SERVICES_WEBSERVERPASSWORD);
  }

  if (!m_webserver->Start(webPort, username, password))
    return false;

#ifdef HAS_ZEROCONF
  std::vector<std::pair<std::string, std::string>> txt;
  txt.emplace_back("txtvers", "1");
  txt.emplace_back("uuid", CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
                               CSettings::SETTING_SERVICES_DEVICEUUID));

  // publish web frontend and API services
  CZeroconf::GetInstance()->PublishService("servers.webserver", "_http._tcp",
                                           CSysInfo::GetDeviceName(), webPort, txt);
  CZeroconf::GetInstance()->PublishService("servers.jsonrpc-http", "_xbmc-jsonrpc-h._tcp",
                                           CSysInfo::GetDeviceName(), webPort, txt);
#endif // HAS_ZEROCONF

  return true;
}

bool CWebServerService::IsWebserverRunning()
{
  return m_webserver->IsStarted();
}

bool CWebServerService::StopWebserver()
{
  if (!IsWebserverRunning())
    return true;

  if (!m_webserver->Stop() || m_webserver->IsStarted())
  {
    CLog::Log(LOGWARNING, "Webserver: Failed to stop.");
    return false;
  }

#ifdef HAS_ZEROCONF
  CZeroconf::GetInstance()->RemoveService("servers.webserver");
  CZeroconf::GetInstance()->RemoveService("servers.jsonrpc-http");
#endif // HAS_ZEROCONF

  return true;
}
