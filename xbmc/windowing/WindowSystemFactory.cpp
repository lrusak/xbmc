/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "WindowSystemFactory.h"

using namespace KODI::WINDOWING;

std::list<std::function<std::unique_ptr<CWinSystemBase>()>> CWindowSystemFactory::m_windowSystems;

std::list<std::function<std::unique_ptr<CWinSystemBase>()>> CWindowSystemFactory::GetWindowSystems()
{
  return m_windowSystems;
}

void CWindowSystemFactory::RegisterWindowSystem(
    std::function<std::unique_ptr<CWinSystemBase>()> createFunction)
{
  m_windowSystems.emplace_back(createFunction);
}

void CWindowSystemFactory::ClearBufferObjects()
{
  m_windowSystems.clear();
}
