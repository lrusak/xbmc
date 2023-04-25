/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "LibraryLoader.h"

#include <vector>

class DllLoaderContainer
{
public:
  static LibraryLoader* GetModule(const char* sName);
  static LibraryLoader* GetModule(const HMODULE hModule);
  static LibraryLoader* LoadModule(const char* sName, const char* sCurrentDir = NULL);
  static void       ReleaseModule(LibraryLoader*& pDll);

  static void RegisterDll(LibraryLoader* pDll);
  static void UnRegisterDll(LibraryLoader* pDll);
};
