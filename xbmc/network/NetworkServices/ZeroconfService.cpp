/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ZeroconfService.h"

#include "ServiceBroker.h"
#include "network/NetworkServices.h"
#include "network/Zeroconf.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/log.h"

void CZeroconfService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CZeroconfService>());
}

CZeroconfService::CZeroconfService()
{
  std::set<std::string> settingSet{CZeroconf::SETTING_SERVICES_ZEROCONF};

  m_settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  m_settings->GetSettingsManager()->RegisterCallback(this, settingSet);
}

bool CZeroconfService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();
  if (settingId == CZeroconf::SETTING_SERVICES_ZEROCONF)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
      return StartZeroconf();
  }

  return true;
}

void CZeroconfService::Start()
{
  StartZeroconf();
}

void CZeroconfService::Stop(bool wait)
{
  StopZeroconf();
}

bool CZeroconfService::StartZeroconf()
{
  if (!m_settings->GetBool(CZeroconf::SETTING_SERVICES_ZEROCONF))
    return false;

  if (IsZeroconfRunning())
    return true;

  CLog::Log(LOGINFO, "starting zeroconf publishing");
  return CZeroconf::GetInstance()->Start();
}

bool CZeroconfService::IsZeroconfRunning()
{
  return CZeroconf::GetInstance()->IsStarted();
}

bool CZeroconfService::StopZeroconf()
{
  if (!IsZeroconfRunning())
    return true;

  CLog::Log(LOGINFO, "stopping zeroconf publishing");
  CZeroconf::GetInstance()->Stop();

  return true;
}
