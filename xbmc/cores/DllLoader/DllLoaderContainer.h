/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "LibraryLoader.h"

#include <memory>
#include <vector>

namespace DllLoaderContainer
{
std::shared_ptr<LibraryLoader> GetModule(const char* sName);
std::shared_ptr<LibraryLoader> GetModule(const HMODULE hModule);
std::shared_ptr<LibraryLoader> LoadModule(const char* sName, const char* sCurrentDir = NULL);
void ReleaseModule(std::shared_ptr<LibraryLoader> pDll);

void RegisterDll(std::shared_ptr<LibraryLoader> pDll);
void UnRegisterDll(std::shared_ptr<LibraryLoader> pDll);
};
