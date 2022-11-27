/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RPProcessInfoGbm.h"

#include "utils/log.h"

using namespace KODI;
using namespace RETRO;

namespace
{

constexpr auto PLATFORM_NAME = "GBM";

}

CRPProcessInfoGbm::CRPProcessInfoGbm() : CRPProcessInfo(PLATFORM_NAME)
{
}

CRPProcessInfo* CRPProcessInfoGbm::Create()
{
  return new CRPProcessInfoGbm();
}

void CRPProcessInfoGbm::Register()
{
  CLog::Log(LOGINFO, "RetroPlayer[PROCESS]: Registering process control for {}", PLATFORM_NAME);

  CRPProcessInfo::RegisterProcessControl(CRPProcessInfoGbm::Create);
}
