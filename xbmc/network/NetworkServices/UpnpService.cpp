/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "UpnpService.h"

#include "ServiceBroker.h"
#include "network/NetworkServices.h"
#include "network/upnp/UPnP.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/lib/SettingsManager.h"
#include "utils/log.h"

void CUpnpService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CUpnpService>());
}

CUpnpService::CUpnpService()
{
  std::set<std::string> settingSet{UPNP::SETTING_SERVICES_UPNP, UPNP::SETTING_SERVICES_UPNPSERVER,
                                   UPNP::SETTING_SERVICES_UPNPRENDERER,
                                   UPNP::SETTING_SERVICES_UPNPCONTROLLER};

  m_settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  m_settings->GetSettingsManager()->RegisterCallback(this, settingSet);
}

bool CUpnpService::OnSettingChanging(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return false;

  const std::string& settingId = setting->GetId();

  if (settingId == UPNP::SETTING_SERVICES_UPNP)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      StartUPnPClient();
      StartUPnPController();
      StartUPnPServer();
      StartUPnPRenderer();
    }
    else
    {
      StopUPnPRenderer();
      StopUPnPServer();
      StopUPnPController();
      StopUPnPClient();
    }
  }
  else if (settingId == UPNP::SETTING_SERVICES_UPNPSERVER)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
    {
      if (!StartUPnPServer())
        return false;

      // always stop and restart the client and controller if necessary
      StopUPnPClient();
      StopUPnPController();
      StartUPnPClient();
      StartUPnPController();
    }
    else
      return StopUPnPServer();
  }
  else if (settingId == UPNP::SETTING_SERVICES_UPNPRENDERER)
  {
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
      return StartUPnPRenderer();
    else
      return StopUPnPRenderer();
  }
  else if (settingId == UPNP::SETTING_SERVICES_UPNPCONTROLLER)
  {
    // always stop and restart
    StopUPnPController();
    if (std::static_pointer_cast<const CSettingBool>(setting)->GetValue())
      return StartUPnPController();
  }

  return true;
}

void CUpnpService::Start()
{
  if (m_settings->GetBool(UPNP::SETTING_SERVICES_UPNP))
    StartUPnP();
}

void CUpnpService::Stop(bool wait)
{
  StopUPnP(wait);
}

bool CUpnpService::StartUPnP()
{
  bool ret = false;

  ret |= StartUPnPClient();
  if (m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPSERVER))
  {
    ret |= StartUPnPServer();
  }

  if (m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPCONTROLLER))
  {
    ret |= StartUPnPController();
  }

  if (m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPRENDERER))
  {
    ret |= StartUPnPRenderer();
  }

  return ret;
}

bool CUpnpService::StopUPnP(bool wait)
{
  if (!UPNP::CUPnP::IsInstantiated())
    return true;

  CLog::Log(LOGINFO, "stopping upnp");
  UPNP::CUPnP::ReleaseInstance(wait);

  return true;
}

bool CUpnpService::StartUPnPClient()
{
  if (!m_settings->GetBool(UPNP::SETTING_SERVICES_UPNP))
    return false;

  CLog::Log(LOGINFO, "starting upnp client");
  UPNP::CUPnP::GetInstance()->StartClient();
  return IsUPnPClientRunning();
}

bool CUpnpService::IsUPnPClientRunning()
{
  return UPNP::CUPnP::GetInstance()->IsClientStarted();
}

bool CUpnpService::StopUPnPClient()
{
  if (!IsUPnPClientRunning())
    return true;

  CLog::Log(LOGINFO, "stopping upnp client");
  UPNP::CUPnP::GetInstance()->StopClient();

  return true;
}

bool CUpnpService::StartUPnPController()
{
  if (!m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPCONTROLLER) ||
      !m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPSERVER) ||
      !m_settings->GetBool(UPNP::SETTING_SERVICES_UPNP))
    return false;

  CLog::Log(LOGINFO, "starting upnp controller");
  UPNP::CUPnP::GetInstance()->StartController();
  return IsUPnPControllerRunning();
}

bool CUpnpService::IsUPnPControllerRunning()
{
  return UPNP::CUPnP::GetInstance()->IsControllerStarted();
}

bool CUpnpService::StopUPnPController()
{
  if (!IsUPnPControllerRunning())
    return true;

  CLog::Log(LOGINFO, "stopping upnp controller");
  UPNP::CUPnP::GetInstance()->StopController();

  return true;
}

bool CUpnpService::StartUPnPRenderer()
{
  if (!m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPRENDERER) ||
      !m_settings->GetBool(UPNP::SETTING_SERVICES_UPNP))
    return false;

  CLog::Log(LOGINFO, "starting upnp renderer");
  return UPNP::CUPnP::GetInstance()->StartRenderer();
}

bool CUpnpService::IsUPnPRendererRunning()
{
  return UPNP::CUPnP::GetInstance()->IsInstantiated();
}

bool CUpnpService::StopUPnPRenderer()
{
  if (!IsUPnPRendererRunning())
    return true;

  CLog::Log(LOGINFO, "stopping upnp renderer");
  UPNP::CUPnP::GetInstance()->StopRenderer();

  return true;
}

bool CUpnpService::StartUPnPServer()
{
  if (!m_settings->GetBool(UPNP::SETTING_SERVICES_UPNPSERVER) ||
      !m_settings->GetBool(UPNP::SETTING_SERVICES_UPNP))
    return false;

  CLog::Log(LOGINFO, "starting upnp server");
  return UPNP::CUPnP::GetInstance()->StartServer();
}

bool CUpnpService::IsUPnPServerRunning()
{
  return UPNP::CUPnP::GetInstance()->IsInstantiated();
}

bool CUpnpService::StopUPnPServer()
{
  if (!IsUPnPServerRunning())
    return true;

  StopUPnPController();

  CLog::Log(LOGINFO, "stopping upnp server");
  UPNP::CUPnP::GetInstance()->StopServer();

  return true;
}
