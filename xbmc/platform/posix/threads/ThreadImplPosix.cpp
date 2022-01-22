/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ThreadImplPosix.h"

#include "utils/log.h"

#ifdef TARGET_DARWIN
#include <pthread.h>
#endif

#if TARGET_FREEBSD
#include <pthread_np.h>
#endif

std::unique_ptr<IThreadImpl> IThreadImpl::CreateThreadImpl(std::thread::native_handle_type handle,
                                                           const std::string& name)
{
  return std::make_unique<CThreadImplPosix>(handle, name);
}

CThreadImplPosix::CThreadImplPosix(std::thread::native_handle_type handle, const std::string& name)
  : IThreadImpl(handle, name)
{
#if defined(TARGET_DARWIN)
  pthread_setname_np(m_name.c_str());
#elif defined(TARGET_FREEBSD)
  pthread_setname_np(m_handle, m_name.c_str());
#endif
}

bool CThreadImplPosix::SetPriority(const ThreadPriority& priority)
{
  CLog::Log(LOGDEBUG, "[THREAD] SetPriority unimplemented for this platform");
}
