/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RecursiveMutex.h"

namespace XbmcThreads
{

CRecursiveMutex::CRecursiveMutex()
{
  pthread_mutex_init(&m_mutex, GetRecursiveAttr());
}

CRecursiveMutex::~CRecursiveMutex()
{
  pthread_mutex_destroy(&m_mutex);
}

void CRecursiveMutex::lock()
{
  pthread_mutex_lock(&m_mutex);
}

void CRecursiveMutex::unlock()
{
  pthread_mutex_unlock(&m_mutex);
}

bool CRecursiveMutex::try_lock()
{
  return (pthread_mutex_trylock(&m_mutex) == 0);
}

static pthread_mutexattr_t recursiveAttr;

static bool SetRecursiveAttr()
{
  static bool alreadyCalled = false;

  if (!alreadyCalled)
  {
    pthread_mutexattr_init(&recursiveAttr);
    pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutexattr_setprotocol(&recursiveAttr, PTHREAD_PRIO_INHERIT);

    alreadyCalled = true;
  }

  return true; // note, we never call destroy.
}

static bool recursiveAttrSet = SetRecursiveAttr();

pthread_mutexattr_t* CRecursiveMutex::GetRecursiveAttr()
{
  if (!recursiveAttrSet) // this is only possible in the single threaded startup code
    recursiveAttrSet = SetRecursiveAttr();

  return &recursiveAttr;
}


} // namespace XbmcThreads
