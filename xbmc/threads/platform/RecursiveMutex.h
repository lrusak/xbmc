/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <mutex>

#if defined(TARGET_POSIX) && !defined(TARGET_ANDROID)
#include <pthread.h>
namespace XbmcThreads
{

/**
 * @brief This class exists purely for the ability to
 *        set mutex attribute PTHREAD_PRIO_INHERIT.
 *        Currently there is no way to set this using
 *        std::recursive_mutex.
 *
 */
class CRecursiveMutex
{
  pthread_mutex_t m_mutex;

  // implementation is in threads/platform/pthreads/ThreadImpl.cpp
  static pthread_mutexattr_t* GetRecursiveAttr();

public:
  CRecursiveMutex(const CRecursiveMutex&) = delete;
  CRecursiveMutex& operator=(const CRecursiveMutex&) = delete;

  CRecursiveMutex();
  ~CRecursiveMutex();

  void lock();
  void unlock();
  bool try_lock();

  std::recursive_mutex::native_handle_type native_handle() { return &m_mutex; }
};
}
#elif defined(TARGET_WINDOWS) || defined(TARGET_ANDROID)
namespace XbmcThreads
{
  typedef std::recursive_mutex CRecursiveMutex;
}
#endif

