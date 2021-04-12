/*
 *  Copyright (C) 2013-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class INetworkService
{
public:
  virtual ~INetworkService() = default;

  virtual std::string Name() = 0;

  virtual void Start() = 0;

  virtual void Stop(bool wait) = 0;

protected:
  INetworkService() = default;
};
