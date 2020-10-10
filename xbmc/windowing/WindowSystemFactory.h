/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "WinSystem.h"

#include <functional>
#include <list>
#include <map>
#include <memory>

namespace KODI
{
namespace WINDOWING
{

class CWindowSystemFactory
{
public:
  static std::function<std::unique_ptr<CWinSystemBase>()> GetWindowSystem(std::string name="default") { return m_windowSystems[name]; }
  static std::map<std::string, std::function<std::unique_ptr<CWinSystemBase>()>> GetWindowSystems();
  static void RegisterWindowSystem(std::string windowSystem, std::function<std::unique_ptr<CWinSystemBase>()> createFunction);
  static void ClearBufferObjects();

private:
  static std::map<std::string, std::function<std::unique_ptr<CWinSystemBase>()>> m_windowSystems;
};

} // namespace WINDOWING
} // namespace KODI
