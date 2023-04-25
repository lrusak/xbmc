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

namespace DllLoaderContainer
{
LibraryLoader* GetModule(const char* sName);
LibraryLoader* GetModule(const HMODULE hModule);
LibraryLoader* LoadModule(const char* sName, const char* sCurrentDir = NULL);
void ReleaseModule(LibraryLoader*& pDll);

void RegisterDll(LibraryLoader* pDll);
void UnRegisterDll(LibraryLoader* pDll);
};
