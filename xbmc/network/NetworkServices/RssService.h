/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "network/NetworkServices/INetworkService.h"

class CNetworkServices;

class CRssService : public INetworkService
{
public:
  CRssService() = default;
  virtual ~CRssService() override = default;

  static void Register(CNetworkServices* networkServices);

  // INetworkService overrides
  std::string Name() override { return "rss"; }
  void Start() override;
  void Stop(bool wait) override;

private:
  bool StartRss();
  bool IsRssRunning();
  bool StopRss();
};
