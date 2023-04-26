/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DllLoaderContainer.h"
#ifdef TARGET_POSIX
#include "SoLoader.h"
#endif
#ifdef TARGET_WINDOWS
#include "Win32DllLoader.h"
#endif
#include "filesystem/File.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "URL.h"

#if defined(TARGET_WINDOWS)
#define ENV_PARTIAL_PATH \
                 "special://xbmcbin/;" \
                 "special://xbmcbin/system/;" \
                 "special://xbmcbin/system/python/;" \
                 "special://xbmc/;" \
                 "special://xbmc/system/;" \
                 "special://xbmc/system/python/"
#else
#define ENV_PARTIAL_PATH \
                 "special://xbmcbin/system/;" \
                 "special://xbmcbin/system/players/mplayer/;" \
                 "special://xbmcbin/system/players/VideoPlayer/;" \
                 "special://xbmcbin/system/players/paplayer/;" \
                 "special://xbmcbin/system/python/;" \
                 "special://xbmc/system/;" \
                 "special://xbmc/system/players/mplayer/;" \
                 "special://xbmc/system/players/VideoPlayer/;" \
                 "special://xbmc/system/players/paplayer/;" \
                 "special://xbmc/system/python/"
#endif
#if defined(TARGET_DARWIN)
#define ENV_PATH ENV_PARTIAL_PATH \
                 ";special://frameworks/"
#else
#define ENV_PATH ENV_PARTIAL_PATH
#endif

//Define this to get logging on all calls to load/unload of dlls
//#define LOGALL

using namespace XFILE;

static std::vector<std::shared_ptr<LibraryLoader>> m_dlls;

namespace
{

bool IsSystemDll(const char* sName)
{
  for (auto& dll : m_dlls)
  {
    if (dll->IsSystemDll() && StringUtils::CompareNoCase(dll->GetName(), sName) == 0)
      return true;
  }

  return false;
}

std::shared_ptr<LibraryLoader> LoadDll(const char* sName)
{

#ifdef LOGALL
  CLog::Log(LOGDEBUG, "Loading dll {}", sName);
#endif

  std::shared_ptr<LibraryLoader> pLoader;
#ifdef TARGET_POSIX
  pLoader = std::make_shared<SoLoader>(sName);
#elif defined(TARGET_WINDOWS)
  pLoader = std::make_shared<Win32DllLoader>(sName, false);
#endif

  if (!pLoader->Load())
  {
    return {};
  }

  return pLoader;
}

std::shared_ptr<LibraryLoader> FindModule(const char* sName, const char* sCurrentDir)
{
  if (URIUtils::IsInArchive(sName))
  {
    CURL url(sName);
    std::string newName = "special://temp/";
    newName += url.GetFileName();
    CFile::Copy(sName, newName);
    return FindModule(newName.c_str(), sCurrentDir);
  }

  if (CURL::IsFullPath(sName))
  { //  Has a path, just try to load
    return LoadDll(sName);
  }
#ifdef TARGET_POSIX
  else if (strcmp(sName, "xbmc.so") == 0)
    return LoadDll(sName);
#endif
  else if (sCurrentDir)
  { // in the path of the parent dll?
    std::string strPath=sCurrentDir;
    strPath+=sName;

    if (CFile::Exists(strPath))
      return LoadDll(strPath.c_str());
  }

  //  in environment variable?
  std::vector<std::string> vecEnv;

#if defined(TARGET_ANDROID)
  std::string systemLibs = getenv("KODI_ANDROID_SYSTEM_LIBS");
  vecEnv = StringUtils::Split(systemLibs, ':');
  std::string localLibs = getenv("KODI_ANDROID_LIBS");
  vecEnv.insert(vecEnv.begin(),localLibs);
#else
  vecEnv = StringUtils::Split(ENV_PATH, ';');
#endif
  std::shared_ptr<LibraryLoader> pDll;

  for (std::vector<std::string>::const_iterator i = vecEnv.begin(); i != vecEnv.end(); ++i)
  {
    std::string strPath = *i;
    URIUtils::AddSlashAtEnd(strPath);

#ifdef LOGALL
    CLog::Log(LOGDEBUG, "Searching for the dll {} in directory {}", sName, strPath);
#endif

    strPath+=sName;

    // Have we already loaded this dll
    if ((pDll = DllLoaderContainer::GetModule(strPath.c_str())) != NULL)
      return pDll;

    if (CFile::Exists(strPath))
      return LoadDll(strPath.c_str());
  }

  // can't find it in any of our paths - could be a system dll
  if ((pDll = LoadDll(sName)) != NULL)
    return pDll;

  CLog::Log(LOGDEBUG, "Dll {} was not found in path", sName);
  return NULL;
}

} // namespace

std::shared_ptr<LibraryLoader> DllLoaderContainer::GetModule(const char* sName)
{
  for (auto& dll : m_dlls)
  {
    if (StringUtils::CompareNoCase(dll->GetName(), sName) == 0)
      return dll;

    if (!dll->IsSystemDll() && StringUtils::CompareNoCase(dll->GetFileName(), sName) == 0)
      return dll;
  }

  return {};
}

std::shared_ptr<LibraryLoader> DllLoaderContainer::GetModule(const HMODULE hModule)
{
  for (auto& dll : m_dlls)
  {
    if (dll->GetHModule() == hModule)
      return dll;
  }

  return {};
}

std::shared_ptr<LibraryLoader> DllLoaderContainer::LoadModule(const char* sName,
                                                              const char* sCurrentDir /*=NULL*/)
{
  std::shared_ptr<LibraryLoader> pDll;

  if (IsSystemDll(sName))
  {
    pDll = GetModule(sName);
  }
  else if (sCurrentDir)
  {
    std::string strPath = sCurrentDir;
    strPath += sName;
    pDll = GetModule(strPath.c_str());
  }

  if (!pDll)
  {
    pDll = GetModule(sName);
  }

  if (!pDll)
  {
    pDll = FindModule(sName, sCurrentDir);
  }
  else if (!pDll->IsSystemDll())
  {
#ifdef LOGALL
    CLog::Log(LOGDEBUG, "Already loaded Dll {} at 0x{:x}", pDll->GetFileName(), pDll);
#endif
  }

  return pDll;
}

void DllLoaderContainer::ReleaseModule(std::shared_ptr<LibraryLoader> pDll)
{
  if (!pDll)
    return;
  if (pDll->IsSystemDll())
  {
    CLog::Log(LOGFATAL, "{} is a system dll and should never be released", pDll->GetName());
    return;
  }

#ifdef LOGALL
    CLog::Log(LOGDEBUG, "Releasing Dll {}", pDll->GetFileName());
#endif

    if (!pDll->HasSymbols())
    {
    pDll->Unload();
  }
  else
    CLog::Log(LOGINFO, "{} has symbols loaded and can never be unloaded", pDll->GetName());
}

void DllLoaderContainer::RegisterDll(std::shared_ptr<LibraryLoader> pDll)
{
  m_dlls.emplace_back(pDll);
}

void DllLoaderContainer::UnRegisterDll(std::shared_ptr<LibraryLoader> pDll)
{
  if (pDll)
  {
    if (pDll->IsSystemDll())
    {
      CLog::Log(LOGFATAL, "{} is a system dll and should never be removed", pDll->GetName());
    }
    else
    {
      m_dlls.erase(std::remove(m_dlls.begin(), m_dlls.end(), pDll), m_dlls.end());
    }
  }
}
