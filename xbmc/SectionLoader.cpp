/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SectionLoader.h"

#include "cores/DllLoader/DllLoaderContainer.h"
#include "utils/GlobalsHandling.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <mutex>

using namespace std::chrono_literals;

namespace
{

//  delay for unloading dll's
constexpr auto UNLOAD_DELAY = 30s;

} // namespace

//Define this to get logging on all calls to load/unload sections/dlls
//#define LOGALL

CSectionLoader& CSectionLoader::GetInstance()
{
  static CSectionLoader s_instance;

  return s_instance;
}

CSectionLoader::CSectionLoader(void) = default;

CSectionLoader::~CSectionLoader(void)
{
  UnloadAll();
}

std::shared_ptr<LibraryLoader> CSectionLoader::LoadDLL(const std::string& dllname,
                                                       bool bDelayUnload /*=true*/)
{
  std::unique_lock<CCriticalSection> lock(m_critSection);

  if (dllname.empty())
    return NULL;

  // check if it's already loaded, and increase the reference count if so
  for (auto& dll : m_vecLoadedDLLs)
  {
    if (StringUtils::EqualsNoCase(dll.m_strDllName, dllname))
    {
      return dll.m_pDll;
    }
  }

  // ok, now load the dll
  CLog::Log(LOGDEBUG, "SECTION:LoadDLL({})", dllname);

  std::shared_ptr<LibraryLoader> pDll(DllLoaderContainer::LoadModule(dllname.c_str(), NULL));
  if (!pDll)
    return {};

  CDll newDLL;
  newDLL.m_strDllName = dllname;
  newDLL.m_bDelayUnload=bDelayUnload;
  newDLL.m_pDll=pDll;
  m_vecLoadedDLLs.emplace_back(newDLL);

  return newDLL.m_pDll;
}

void CSectionLoader::UnloadDLL(const std::string &dllname)
{
  std::unique_lock<CCriticalSection> lock(m_critSection);

  if (dllname.empty())
    return;

  // check if it's already loaded, and decrease the reference count if so
  m_vecLoadedDLLs.erase(std::remove_if(m_vecLoadedDLLs.begin(), m_vecLoadedDLLs.end(),
                                       [&dllname](auto& dll)
                                       {
                                         if (!StringUtils::EqualsNoCase(dll.m_strDllName, dllname))
                                           return false;

                                         if (dll.m_pDll.use_count() > 1)
                                           return false;

                                         if (dll.m_bDelayUnload)
                                           dll.m_unloadDelayStartTick =
                                               std::chrono::steady_clock::now();
                                         else
                                         {
                                           CLog::Log(LOGDEBUG, "SECTION:UnloadDll({})", dllname);
                                           if (dll.m_pDll)
                                             DllLoaderContainer::ReleaseModule(dll.m_pDll);

                                           return true;
                                         }

                                         return false;
                                       }),
                        m_vecLoadedDLLs.end());
}

void CSectionLoader::UnloadDelayed()
{
  std::unique_lock<CCriticalSection> lock(m_critSection);

  // check if we can unload any unreferenced dlls
  m_vecLoadedDLLs.erase(
      std::remove_if(m_vecLoadedDLLs.begin(), m_vecLoadedDLLs.end(),
                     [](auto& dll)
                     {
                       auto now = std::chrono::steady_clock::now();
                       auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - dll.m_unloadDelayStartTick);

                       if (dll.m_pDll.use_count() == 1 && duration > UNLOAD_DELAY)
                       {
                         CLog::Log(LOGDEBUG, "SECTION:UnloadDelayed(DLL: {})", dll.m_strDllName);

                         if (dll.m_pDll)
                           DllLoaderContainer::ReleaseModule(dll.m_pDll);

                         return true;
                       }

                       return false;
                     }),
      m_vecLoadedDLLs.end());
}

void CSectionLoader::UnloadAll()
{
  // delete the dll's
  std::unique_lock<CCriticalSection> lock(m_critSection);

  for (auto& dll : m_vecLoadedDLLs)
  {
    if (dll.m_pDll)
      DllLoaderContainer::ReleaseModule(dll.m_pDll);
  }

  m_vecLoadedDLLs.clear();
}
