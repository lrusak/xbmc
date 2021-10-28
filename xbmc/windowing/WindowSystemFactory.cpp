/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "WindowSystemFactory.h"

#include <algorithm>

using namespace KODI::WINDOWING;

std::list<CWindowSystemFactory::Registration>
    CWindowSystemFactory::m_registration;

std::list<std::string> CWindowSystemFactory::GetWindowSystems()
{
  std::list<std::string> available;
  for (const auto& registration : m_registration)
    available.emplace_back(registration.windowSystem);

  return available;
}

std::unique_ptr<CWinSystemBase> CWindowSystemFactory::CreateWindowSystem(
    const std::string& windowSystem)
{
  auto registration = std::find_if(m_registration.cbegin(), m_registration.cend(),
                                   [&windowSystem](auto& registration)
                                   { return registration.windowSystem == windowSystem; });
  if (registration != m_registration.end())
    return registration->createFunction();

  return nullptr;
}

void CWindowSystemFactory::RegisterWindowSystem(
    const std::function<std::unique_ptr<CWinSystemBase>()>& createFunction,
    const std::string& windowSystem)
{
  Registration registration = {};
  registration.createFunction = createFunction;
  registration.windowSystem = windowSystem;

  m_registration.emplace_back(registration);
}
