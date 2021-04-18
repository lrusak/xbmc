/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JsonServerService.h"

#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "interfaces/json-rpc/JSONRPC.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "network/EventServer.h"
#include "network/NetworkServices.h"
#include "network/TCPServer.h"
#include "network/Zeroconf.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/SystemInfo.h"
#include "utils/log.h"

void CJsonServerService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CJsonServerService>());
}

CJsonServerService::CJsonServerService()
{
  std::set<std::string> settingSet{EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED,
                                   EVENTSERVER::CEventServer::SETTING_SERVICES_ESALLINTERFACES};

  m_settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  m_settings->GetSettingsManager()->RegisterCallback(this, settingSet);
}

bool CJsonServerService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();

  if (settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      bool result = true;
      if (!StartJSONRPCServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        result = false;
      }
      return result;
    }
    else
    {
      bool result = true;
      result = StopJSONRPCServer(false);
      return result;
    }
  }
  else if (settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESALLINTERFACES)
  {
    if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESALLINTERFACES) &&
        KODI::MESSAGING::HELPERS::ShowYesNoDialogText(19098, 36633) !=
            KODI::MESSAGING::HELPERS::DialogResponse::YES)
    {
      // Revert change, do not start server
      return false;
    }

    if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
    {
      if (!StopJSONRPCServer(true))
        return false;

      if (!StartJSONRPCServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        return false;
      }
    }
  }

  return true;
}

void CJsonServerService::Start()
{
  if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED) &&
      !StartJSONRPCServer())
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33103),
                                          g_localizeStrings.Get(33100));
}

void CJsonServerService::Stop(bool wait)
{
  StopJSONRPCServer(wait);
}

bool CJsonServerService::StartJSONRPCServer()
{
  if (!m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (IsJSONRPCServerRunning())
    return true;

  if (!JSONRPC::CTCPServer::StartServer(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_jsonTcpPort,
          m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESALLINTERFACES)))
    return false;

#ifdef HAS_ZEROCONF
  std::vector<std::pair<std::string, std::string>> txt;
  txt.emplace_back("txtvers", "1");
  txt.emplace_back("uuid", CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
                               CSettings::SETTING_SERVICES_DEVICEUUID));

  CZeroconf::GetInstance()->PublishService(
      "servers.jsonrpc-tpc", "_xbmc-jsonrpc._tcp", CSysInfo::GetDeviceName(),
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_jsonTcpPort, txt);
#endif // HAS_ZEROCONF

  return true;
}

bool CJsonServerService::IsJSONRPCServerRunning()
{
  return JSONRPC::CTCPServer::IsRunning();
}

bool CJsonServerService::StopJSONRPCServer(bool wait)
{
  if (!IsJSONRPCServerRunning())
    return true;

  JSONRPC::CTCPServer::StopServer(wait);

#ifdef HAS_ZEROCONF
  CZeroconf::GetInstance()->RemoveService("servers.jsonrpc-tcp");
#endif // HAS_ZEROCONF

  return true;
}
