/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RssService.h"

#include "ServiceBroker.h"
#include "network/NetworkServices.h"
#include "utils/RssManager.h"

void CRssService::Register(CNetworkServices* networkServices)
{
  networkServices->RegisterService(std::make_unique<CRssService>());
}

void CRssService::Start()
{
  StartRss();
}

void CRssService::Stop(bool wait)
{
  StopRss();
}

bool CRssService::StartRss()
{
  if (IsRssRunning())
    return true;

  CRssManager::GetInstance().Start();
  return true;
}

bool CRssService::IsRssRunning()
{
  return CRssManager::GetInstance().IsActive();
}

bool CRssService::StopRss()
{
  if (!IsRssRunning())
    return true;

  CRssManager::GetInstance().Stop();
  return true;
}
