/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Job.h"

#include "JobManager.h"
#include "utils/log.h"

bool CJob::ShouldCancel(unsigned int progress, unsigned int total) const
{
  if (m_manager)
    return m_manager->OnJobProgress(progress, total, this);

  return false;
}

void CJob::CancelJob()
{
  if (m_manager)
    m_manager->CancelJob(this->GetJobID());

  this->SetJobID(0);

  CLog::Log(LOGDEBUG, "CancelJob: {} id: {}", fmt::ptr(this), this->GetJobID());
}

void CJob::FreeJob()
{
  CLog::Log(LOGDEBUG, "FreeJob: {} id: {}", fmt::ptr(this), this->GetJobID());

  delete this;
}
