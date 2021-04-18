/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "NetworkServices.h"

#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "interfaces/json-rpc/JSONRPC.h"
#include "messaging/ApplicationMessenger.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "network/EventServer.h"
#include "network/Network.h"
#include "network/NetworkServices/INetworkService.h"
#include "network/NetworkServices/RssService.h"
#include "network/TCPServer.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/RssManager.h"
#include "utils/SystemInfo.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <utility>

#ifdef TARGET_LINUX
#include "Util.h"
#endif

#if defined(HAS_AIRPLAY) || defined(HAS_AIRTUNES)
#include "network/NetworkServices/AirPlayService.h"
#endif

#ifdef HAS_AIRPLAY
#include "network/AirPlayServer.h"
#endif // HAS_AIRPLAY

#ifdef HAS_ZEROCONF
#include "network/NetworkServices/ZeroconfService.h"
#include "network/Zeroconf.h"
#endif // HAS_ZEROCONF

#ifdef HAS_UPNP
#include "network/NetworkServices/UpnpService.h"
#include "network/upnp/UPnP.h"
#endif // HAS_UPNP

#ifdef HAS_WEB_SERVER
#include "network/NetworkServices/WebServerService.h"
#include "network/WebServer.h"
#endif // HAS_WEB_SERVER

#if defined(TARGET_DARWIN_OSX)
#include "platform/darwin/osx/XBMCHelper.h"
#endif

using namespace KODI::MESSAGING;
using namespace JSONRPC;
using namespace EVENTSERVER;

using KODI::MESSAGING::HELPERS::DialogResponse;

CNetworkServices::CNetworkServices()
{
  std::set<std::string> settingSet{
#if HAS_FILESYSTEM_SMB
        SMB::SETTING_SMB_WINSSERVER, SMB::SETTING_SMB_WORKGROUP, SMB::SETTING_SMB_MINPROTOCOL,
        SMB::SETTING_SMB_MAXPROTOCOL, SMB::SETTING_SMB_LEGACYSECURITY,
#endif
        CEventServer::SETTING_SERVICES_ESENABLED, CEventServer::SETTING_SERVICES_ESPORT,
        CEventServer::SETTING_SERVICES_ESALLINTERFACES,
        CEventServer::SETTING_SERVICES_ESINITIALDELAY,
        CEventServer::SETTING_SERVICES_ESCONTINUOUSDELAY
  };
  m_settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  m_settings->GetSettingsManager()->RegisterCallback(this, settingSet);

#ifdef HAS_ZEROCONF
  CZeroconfService::Register(this);
#endif

#ifdef HAS_WEB_SERVER
  CWebServerService::Register(this);
#endif
#if defined(HAS_AIRPLAY) || defined(HAS_AIRTUNES)
  CAirPlayService::Register(this);
#endif

#ifdef HAS_UPNP
  CUpnpService::Register(this);
#endif

  CRssService::Register(this);
}

CNetworkServices::~CNetworkServices()
{
}

void CNetworkServices::RegisterService(std::unique_ptr<INetworkService> service)
{
  m_services.emplace_back(std::move(service));
}

bool CNetworkServices::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (setting == NULL)
    return false;

  const std::string &settingId = setting->GetId();

  if (settingId == CEventServer::SETTING_SERVICES_ESENABLED)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      bool result = true;
      if (!StartEventServer())
      {
        HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
        result = false;
      }

      if (!StartJSONRPCServer())
      {
        HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        result = false;
      }
      return result;
    }
    else
    {
      bool result = true;
      result = StopEventServer(true, true);
      result &= StopJSONRPCServer(false);
      return result;
    }
  }
  else if (settingId == CEventServer::SETTING_SERVICES_ESPORT)
  {
    // restart eventserver without asking user
    if (!StopEventServer(true, false))
      return false;

    if (!StartEventServer())
    {
      HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
      return false;
    }

#if defined(TARGET_DARWIN_OSX)
    // reconfigure XBMCHelper for port changes
    XBMCHelper::GetInstance().Configure();
#endif // TARGET_DARWIN_OSX
  }
  else if (settingId == CEventServer::SETTING_SERVICES_ESALLINTERFACES)
  {
    if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESALLINTERFACES) &&
        HELPERS::ShowYesNoDialogText(19098, 36633) != DialogResponse::YES)
    {
      // Revert change, do not start server
      return false;
    }

    if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
    {
      if (!StopEventServer(true, true))
        return false;

      if (!StartEventServer())
      {
        HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
        return false;
      }
    }

    if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
    {
      if (!StopJSONRPCServer(true))
        return false;

      if (!StartJSONRPCServer())
      {
        HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        return false;
      }
    }
  }

  else if (settingId == CEventServer::SETTING_SERVICES_ESINITIALDELAY ||
           settingId == CEventServer::SETTING_SERVICES_ESCONTINUOUSDELAY)
  {
    if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
      return RefreshEventServer();
  }

  return true;
}

void CNetworkServices::OnSettingChanged(const std::shared_ptr<const CSetting>& setting)
{
  if (setting == NULL)
    return;

#ifdef HAS_FILESYSTEM_SMB
  const std::string& settingId = setting->GetId();
  if (settingId == SMB::SETTING_SMB_WINSSERVER || settingId == SMB::SETTING_SMB_WORKGROUP ||
      settingId == SMB::SETTING_SMB_MINPROTOCOL || settingId == SMB::SETTING_SMB_MAXPROTOCOL ||
      settingId == SMB::SETTING_SMB_LEGACYSECURITY)
  {
    // okey we really don't need to restart, only deinit samba, but that could be damn hard if something is playing
    //! @todo - General way of handling setting changes that require restart
    if (HELPERS::ShowYesNoDialogText(CVariant{14038}, CVariant{14039}) == DialogResponse::YES)
    {
      m_settings->Save();
      CApplicationMessenger::GetInstance().PostMsg(TMSG_RESTARTAPP);
    }
  }
#endif
}

void CNetworkServices::Start()
{

  if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED) && !StartEventServer())
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33102), g_localizeStrings.Get(33100));
  if (m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED) && !StartJSONRPCServer())
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33103), g_localizeStrings.Get(33100));

  for (const auto& service : m_services)
    service->Start();
}

void CNetworkServices::Stop(bool bWait)
{
  for (const auto& service : m_services)
    service->Stop(bWait);

  StopEventServer(bWait, false);
  StopJSONRPCServer(bWait);
}

bool CNetworkServices::StartServer(enum ESERVERS server, bool start)
{
  auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (!settingsComponent)
    return false;

  auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  if (!settings)
    return false;

  bool ret = false;
  switch (server)
  {
#ifdef HAS_WEB_SERVER
    case ES_WEBSERVER:
      // the callback will take care of starting/stopping webserver
      ret = settings->SetBool(CWebServer::SETTING_SERVICES_WEBSERVER, start);
      break;
#endif
#ifdef HAS_AIRPLAY
    case ES_AIRPLAYSERVER:
      // the callback will take care of starting/stopping airplay
      ret = settings->SetBool(CAirPlayServer::SETTING_SERVICES_AIRPLAY, start);
      break;
#endif
    case ES_JSONRPCSERVER:
      // the callback will take care of starting/stopping jsonrpc server
      ret = settings->SetBool(CEventServer::SETTING_SERVICES_ESENABLED, start);
      break;
#ifdef HAS_UPNP
    case ES_UPNPSERVER:
      // the callback will take care of starting/stopping upnp server
      ret = settings->SetBool(UPNP::SETTING_SERVICES_UPNPSERVER, start);
      break;

    case ES_UPNPRENDERER:
      // the callback will take care of starting/stopping upnp renderer
      ret = settings->SetBool(UPNP::SETTING_SERVICES_UPNPRENDERER, start);
      break;
#endif
    case ES_EVENTSERVER:
      // the callback will take care of starting/stopping event server
      ret = settings->SetBool(CEventServer::SETTING_SERVICES_ESENABLED, start);
      break;
#ifdef HAS_ZEROCONF
    case ES_ZEROCONF:
      // the callback will take care of starting/stopping zeroconf
      ret = settings->SetBool(CZeroconf::SETTING_SERVICES_ZEROCONF, start);
      break;
#endif
    default:
      ret = false;
      break;
  }
  settings->Save();

  return ret;
}

bool CNetworkServices::StartJSONRPCServer()
{
  if (!m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (IsJSONRPCServerRunning())
    return true;

  if (!CTCPServer::StartServer(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_jsonTcpPort,
          m_settings->GetBool(CEventServer::SETTING_SERVICES_ESALLINTERFACES)))
    return false;

#ifdef HAS_ZEROCONF
  std::vector<std::pair<std::string, std::string> > txt;
  txt.emplace_back("txtvers", "1");
  txt.emplace_back("uuid", CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
                             CSettings::SETTING_SERVICES_DEVICEUUID));

  CZeroconf::GetInstance()->PublishService("servers.jsonrpc-tpc", "_xbmc-jsonrpc._tcp", CSysInfo::GetDeviceName(), CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_jsonTcpPort, txt);
#endif // HAS_ZEROCONF

  return true;
}

bool CNetworkServices::IsJSONRPCServerRunning()
{
  return CTCPServer::IsRunning();
}

bool CNetworkServices::StopJSONRPCServer(bool bWait)
{
  if (!IsJSONRPCServerRunning())
    return true;

  CTCPServer::StopServer(bWait);

#ifdef HAS_ZEROCONF
  CZeroconf::GetInstance()->RemoveService("servers.jsonrpc-tcp");
#endif // HAS_ZEROCONF

  return true;
}

bool CNetworkServices::StartEventServer()
{
  if (!m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (IsEventServerRunning())
    return true;

  CEventServer* server = CEventServer::GetInstance();
  if (!server)
  {
    CLog::Log(LOGERROR, "ES: Out of memory");
    return false;
  }

  server->StartServer();

  return true;
}

bool CNetworkServices::IsEventServerRunning()
{
  return CEventServer::GetInstance()->Running();
}

bool CNetworkServices::StopEventServer(bool bWait, bool promptuser)
{
  if (!IsEventServerRunning())
    return true;

  CEventServer* server = CEventServer::GetInstance();
  if (!server)
  {
    CLog::Log(LOGERROR, "ES: Out of memory");
    return false;
  }

  if (promptuser)
  {
    if (server->GetNumberOfClients() > 0)
    {
      if (HELPERS::ShowYesNoDialogText(CVariant{13140}, CVariant{13141}, CVariant{""}, CVariant{""}, 10000) !=
        DialogResponse::YES)
      {
        CLog::Log(LOGINFO, "ES: Not stopping event server");
        return false;
      }
    }
    CLog::Log(LOGINFO, "ES: Stopping event server with confirmation");

    CEventServer::GetInstance()->StopServer(true);
  }
  else
  {
    if (!bWait)
      CLog::Log(LOGINFO, "ES: Stopping event server");

    CEventServer::GetInstance()->StopServer(bWait);
  }

  return true;
}

bool CNetworkServices::RefreshEventServer()
{
  if (!m_settings->GetBool(CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (!IsEventServerRunning())
    return false;

  CEventServer::GetInstance()->RefreshSettings();
  return true;
}

bool CNetworkServices::ValidatePort(int port)
{
  if (port <= 0 || port > 65535)
    return false;

#ifdef TARGET_LINUX
  if (!CUtil::CanBindPrivileged() && (port < 1024 || port > 65535))
    return false;
#endif

  return true;
}
