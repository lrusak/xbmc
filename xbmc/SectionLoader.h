/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

//  forward
class LibraryLoader;

class CSectionLoader
{
public:
  class CDll
  {
  public:
    std::string m_strDllName;
    std::shared_ptr<LibraryLoader> m_pDll;
    std::chrono::time_point<std::chrono::steady_clock> m_unloadDelayStartTick;
    bool m_bDelayUnload;
  };

  static CSectionLoader& GetInstance();

  CSectionLoader(void);
  virtual ~CSectionLoader(void);

  std::shared_ptr<LibraryLoader> LoadDLL(const std::string& strSection, bool bDelayUnload = true);
  void UnloadDLL(const std::string& strSection);
  void UnloadDelayed();
  void UnloadAll();

private:
  std::vector<CDll> m_vecLoadedDLLs;
  CCriticalSection m_critSection;
};
