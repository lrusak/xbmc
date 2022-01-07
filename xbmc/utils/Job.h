/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IJob.h"

#include <cstdint>
#include <stddef.h>

#define kJobTypeMediaFlags  "mediaflags"
#define kJobTypeCacheImage  "cacheimage"
#define kJobTypeDDSCompress "ddscompress"


class CJobManager;

/*!
 \ingroup jobs
 \brief Base class for jobs that are executed asynchronously.

 Clients of the CJobManager should subclass CJob and provide the DoWork() function. Data should be
 passed to the job on creation, and any data sharing between the job and the client should be kept to within
 the callback functions if possible, and guarded with critical sections as appropriate.

 Jobs typically fall into two groups: small jobs that perform a single function, and larger jobs that perform a
 sequence of functions.  Clients with small jobs should implement the IJobCallback::OnJobComplete() callback to receive results.
 Clients with larger jobs may wish to implement both the IJobCallback::OnJobComplete() and IJobCallback::OnJobProgress()
 callbacks to receive updates.  Jobs may be cancelled at any point by the client via CJobManager::CancelJob(), however
 effort should be taken to ensure that any callbacks and cancellation is suitably guarded against simultaneous thread access.

 Handling cancellation of jobs within the OnJobProgress callback is a threadsafe operation, as all execution is
 then in the Job thread.

 \sa CJobManager and IJobCallback
 */
class CJob : public IJob
{
public:
  CJob() = default;

  /*!
   \brief Destructor for job objects.

   Jobs are destroyed by the CJobManager after the OnJobComplete() or OnJobAbort() callback is
   complete.  CJob subclasses should therefore supply a virtual destructor to cleanup any memory
   allocated by complete or cancelled jobs.

   \sa CJobManager
   */
  virtual ~CJob() override = default;

  virtual bool operator==(const CJob* job) const
  {
    return false;
  }

  virtual const char* GetType() const override { return "CJob"; }

  /*!
   \brief Function for longer jobs to report progress and check whether they have been cancelled.

   Jobs that contain loops that may take time should check this routine each iteration of the loop,
   both to (optionally) report progress, and to check for cancellation.

   \param progress the amount of the job performed, out of total.
   \param total the total amount of processing to be performed
   \return if true, the job has been asked to cancel.

   \sa IJobCallback::OnJobProgress()
   */
  virtual bool ShouldCancel(unsigned int progress, unsigned int total) const override;

  void CancelJob() override;

  void FreeJob() override;

  void SetJobID(const uint32_t id) override { m_id = id; }

  uint32_t GetJobID() const override { return m_id; }

  void SetCallback(IJobCallback* callback) override { m_callback = callback; }

  IJobCallback* GetCallback() const override { return m_callback; }

  void SetPriority(const JobPriority& priority) override { m_priority = priority; }

  JobPriority GetPriority() const override { return m_priority; }

  void SetJobManager(CJobManager* manager) override { m_manager = manager; }

protected:
  friend class CJobManager;

private:
  IJobCallback* m_callback{nullptr};
  CJobManager* m_manager{nullptr};

  uint32_t m_id{0};
  JobPriority m_priority{JobPriority::LOW};
};
