/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EventServerService.h"

#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/LocalizeStrings.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "network/EventServer.h"
#include "network/Network.h"
#include "network/NetworkServices.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/log.h"


void CEventServerService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CEventServerService>());
}

CEventServerService::CEventServerService()
{
  std::set<std::string> settingSet{EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED,
                                   EVENTSERVER::CEventServer::SETTING_SERVICES_ESPORT,
                                   EVENTSERVER::CEventServer::SETTING_SERVICES_ESALLINTERFACES,
                                   EVENTSERVER::CEventServer::SETTING_SERVICES_ESINITIALDELAY,
                                   EVENTSERVER::CEventServer::SETTING_SERVICES_ESCONTINUOUSDELAY};

  m_settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  m_settings->GetSettingsManager()->RegisterCallback(this, settingSet);
}

bool CEventServerService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();

  if (settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      bool result = true;
      if (!StartEventServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
        result = false;
      }

      if (!CServiceBroker::GetNetwork().GetServices().StartJSONRPCServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        result = false;
      }
      return result;
    }
    else
    {
      bool result = true;
      result = StopEventServer(true, true);
      result &= CServiceBroker::GetNetwork().GetServices().StopJSONRPCServer(false);
      return result;
    }
  }
  else if (settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESPORT)
  {
    // restart eventserver without asking user
    if (!StopEventServer(true, false))
      return false;

    if (!StartEventServer())
    {
      KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
      return false;
    }

#if defined(TARGET_DARWIN_OSX)
    // reconfigure XBMCHelper for port changes
    XBMCHelper::GetInstance().Configure();
#endif // TARGET_DARWIN_OSX
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
      if (!StopEventServer(true, true))
        return false;

      if (!StartEventServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33102}, CVariant{33100});
        return false;
      }
    }

    if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
    {
      if (!CServiceBroker::GetNetwork().GetServices().StopJSONRPCServer(true))
        return false;

      if (!CServiceBroker::GetNetwork().GetServices().StartJSONRPCServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{33103}, CVariant{33100});
        return false;
      }
    }
  }

  else if (settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESINITIALDELAY ||
           settingId == EVENTSERVER::CEventServer::SETTING_SERVICES_ESCONTINUOUSDELAY)
  {
    if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
      return RefreshEventServer();
  }

  return true;
}

void CEventServerService::Start()
{
  if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED) &&
      !StartEventServer())
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33102),
                                          g_localizeStrings.Get(33100));
  if (m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED) &&
      !CServiceBroker::GetNetwork().GetServices().StartJSONRPCServer())
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(33103),
                                          g_localizeStrings.Get(33100));
}

void CEventServerService::Stop(bool wait)
{
  StopEventServer(wait, false);
}

bool CEventServerService::StartEventServer()
{
  if (!m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (IsEventServerRunning())
    return true;

  auto server = EVENTSERVER::CEventServer::GetInstance();
  if (!server)
  {
    CLog::Log(LOGERROR, "ES: Out of memory");
    return false;
  }

  server->StartServer();

  return true;
}

bool CEventServerService::IsEventServerRunning()
{
  return EVENTSERVER::CEventServer::GetInstance()->Running();
}

bool CEventServerService::StopEventServer(bool wait, bool promptuser)
{
  if (!IsEventServerRunning())
    return true;

  auto server = EVENTSERVER::CEventServer::GetInstance();
  if (!server)
  {
    CLog::Log(LOGERROR, "ES: Out of memory");
    return false;
  }

  if (promptuser)
  {
    if (server->GetNumberOfClients() > 0)
    {
      if (KODI::MESSAGING::HELPERS::ShowYesNoDialogText(CVariant{13140}, CVariant{13141},
                                                        CVariant{""}, CVariant{""}, 10000) !=
          KODI::MESSAGING::HELPERS::DialogResponse::YES)
      {
        CLog::Log(LOGINFO, "ES: Not stopping event server");
        return false;
      }
    }
    CLog::Log(LOGINFO, "ES: Stopping event server with confirmation");

    EVENTSERVER::CEventServer::GetInstance()->StopServer(true);
  }
  else
  {
    if (!wait)
      CLog::Log(LOGINFO, "ES: Stopping event server");

    EVENTSERVER::CEventServer::GetInstance()->StopServer(wait);
  }

  return true;
}

bool CEventServerService::RefreshEventServer()
{
  if (!m_settings->GetBool(EVENTSERVER::CEventServer::SETTING_SERVICES_ESENABLED))
    return false;

  if (!IsEventServerRunning())
    return false;

  EVENTSERVER::CEventServer::GetInstance()->RefreshSettings();
  return true;
}
