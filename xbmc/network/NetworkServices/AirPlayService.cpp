/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AirPlayService.h"

#include "ServiceBroker.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "network/NetworkServices.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/SystemInfo.h"

#ifdef HAS_AIRPLAY
#include "network/AirPlayServer.h"
#endif //HAS_AIRPLAY
#ifdef HAS_AIRTUNES
#include "network/AirTunesServer.h"
#endif //HAS_AIRTUNES

#include "network/Zeroconf.h"
#include "utils/log.h"

void CAirPlayService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CAirPlayService>());
}

CAirPlayService::CAirPlayService()
{
#ifdef HAS_AIRPLAY
  std::set<std::string> settingSet{CAirPlayServer::SETTING_SERVICES_AIRPLAY,
                                   CAirPlayServer::SETTING_SERVICES_AIRPLAYVOLUMECONTROL,
                                   CAirPlayServer::SETTING_SERVICES_AIRPLAYVIDEOSUPPORT,
                                   CAirPlayServer::SETTING_SERVICES_USEAIRPLAYPASSWORD,
                                   CAirPlayServer::SETTING_SERVICES_AIRPLAYPASSWORD};

  auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (!settingsComponent)
    throw std::runtime_error("CAirPlayService: CSettingsComponent isn't available!");

  m_settings = settingsComponent->GetSettings();
  if (!m_settings)
    throw std::runtime_error("CAirPlayService: CSettings isn't available!");

  auto settingsManager = m_settings->GetSettingsManager();
  if (!settingsManager)
    throw std::runtime_error("CAirPlayService: CSettingsManager isn't available!");

  settingsManager->RegisterCallback(this, settingSet);
#endif //HAS_AIRPLAY
}

bool CAirPlayService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();
#ifdef HAS_AIRPLAY
  if (settingId == CAirPlayServer::SETTING_SERVICES_AIRPLAY)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      // AirPlay needs zeroconf
      if (!m_settings->GetBool(CZeroconf::SETTING_SERVICES_ZEROCONF))
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{1273}, CVariant{34302});
        return false;
      }

      // note - airtunesserver has to start before airplay server (ios7 client detection bug)
#ifdef HAS_AIRTUNES
      if (!StartAirTunesServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{1274}, CVariant{33100});
        return false;
      }
#endif //HAS_AIRTUNES

      if (!StartAirPlayServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{1273}, CVariant{33100});
        return false;
      }
    }
    else
    {
      bool ret = true;
#ifdef HAS_AIRTUNES
      if (!StopAirTunesServer(true))
        ret = false;
#endif //HAS_AIRTUNES

      if (!StopAirPlayServer(true))
        ret = false;

      if (!ret)
        return false;
    }
  }
  else if (settingId == CAirPlayServer::SETTING_SERVICES_AIRPLAYVIDEOSUPPORT)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      if (!StartAirPlayServer())
      {
        KODI::MESSAGING::HELPERS::ShowOKDialogText(CVariant{1273}, CVariant{33100});
        return false;
      }
    }
    else
    {
      if (!StopAirPlayServer(true))
        return false;
    }
  }
  else if (settingId == CAirPlayServer::SETTING_SERVICES_AIRPLAYPASSWORD ||
           settingId == CAirPlayServer::SETTING_SERVICES_USEAIRPLAYPASSWORD)
  {
    if (!m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_AIRPLAY))
      return false;

    if (!CAirPlayServer::SetCredentials(
            m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_USEAIRPLAYPASSWORD),
            m_settings->GetString(CAirPlayServer::SETTING_SERVICES_AIRPLAYPASSWORD)))
      return false;
  }
#endif //HAS_AIRPLAY

  return true;
}

void CAirPlayService::Start()
{
  // note - airtunesserver has to start before airplay server (ios7 client detection bug)
  StartAirTunesServer();
  StartAirPlayServer();
}

void CAirPlayService::Stop(bool wait)
{
  StopAirPlayServer(wait);
  StopAirTunesServer(wait);
}

bool CAirPlayService::StartAirPlayServer()
{
  if (!m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_AIRPLAYVIDEOSUPPORT))
    return true;

#ifdef HAS_AIRPLAY
  if (!CServiceBroker::GetNetwork().IsAvailable() ||
      !m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_AIRPLAY))
    return false;

  if (IsAirPlayServerRunning())
    return true;

  if (!CAirPlayServer::StartServer(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_airPlayPort, true))
    return false;

  if (!CAirPlayServer::SetCredentials(
          m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_USEAIRPLAYPASSWORD),
          m_settings->GetString(CAirPlayServer::SETTING_SERVICES_AIRPLAYPASSWORD)))
    return false;

  std::vector<std::pair<std::string, std::string>> txt;
  CNetworkInterface* iface = CServiceBroker::GetNetwork().GetFirstConnectedInterface();
  txt.emplace_back("deviceid", iface != nullptr ? iface->GetMacAddress() : "FF:FF:FF:FF:FF:F2");
  txt.emplace_back("model", "Xbmc,1");
  txt.emplace_back("srcvers", AIRPLAY_SERVER_VERSION_STR);

  // for ios8 clients we need to announce mirroring support
  // else we won't get video urls anymore.
  // We also announce photo caching support (as it seems faster and
  // we have implemented it anyways).
  txt.emplace_back("features", "0x20F7");

  CZeroconf::GetInstance()->PublishService(
      "servers.airplay", "_airplay._tcp", CSysInfo::GetDeviceName(),
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_airPlayPort, txt);

  return true;
#endif // HAS_AIRPLAY
  return false;
}

bool CAirPlayService::IsAirPlayServerRunning()
{
#ifdef HAS_AIRPLAY
  return CAirPlayServer::IsRunning();
#endif // HAS_AIRPLAY
  return false;
}

bool CAirPlayService::StopAirPlayServer(bool bWait)
{
#ifdef HAS_AIRPLAY
  if (!IsAirPlayServerRunning())
    return true;

  CAirPlayServer::StopServer(bWait);

  CZeroconf::GetInstance()->RemoveService("servers.airplay");

  return true;
#endif // HAS_AIRPLAY
  return false;
}

bool CAirPlayService::StartAirTunesServer()
{
#ifdef HAS_AIRTUNES
  if (!CServiceBroker::GetNetwork().IsAvailable() ||
      !m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_AIRPLAY))
    return false;

  if (IsAirTunesServerRunning())
    return true;

  if (!CAirTunesServer::StartServer(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_airTunesPort, true,
          m_settings->GetBool(CAirPlayServer::SETTING_SERVICES_USEAIRPLAYPASSWORD),
          m_settings->GetString(CAirPlayServer::SETTING_SERVICES_AIRPLAYPASSWORD)))
  {
    CLog::Log(LOGERROR, "Failed to start AirTunes Server");
    return false;
  }

  return true;
#endif // HAS_AIRTUNES
  return false;
}

bool CAirPlayService::IsAirTunesServerRunning()
{
#ifdef HAS_AIRTUNES
  return CAirTunesServer::IsRunning();
#endif // HAS_AIRTUNES
  return false;
}

bool CAirPlayService::StopAirTunesServer(bool bWait)
{
#ifdef HAS_AIRTUNES
  if (!IsAirTunesServerRunning())
    return true;

  CAirTunesServer::StopServer(bWait);
  return true;
#endif // HAS_AIRTUNES
  return false;
}
