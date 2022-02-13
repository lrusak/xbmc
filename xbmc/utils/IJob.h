/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>

enum class JobPriority;
class CJobManager;
class IJobCallback;

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
class IJob
{
public:
  IJob() = default;

  /*!
   \brief Destructor for job objects.

   Jobs are destroyed by the CJobManager after the OnJobComplete() or OnJobAbort() callback is
   complete.  CJob subclasses should therefore supply a virtual destructor to cleanup any memory
   allocated by complete or cancelled jobs.

   \sa CJobManager
   */
  virtual ~IJob() = default;

  /*!
   \brief Main workhorse function of CJob instances

   All CJob subclasses must implement this function, performing all processing.  Once this function
   is complete, the OnJobComplete() callback is called, and the job is then destroyed.

   \sa CJobManager, IJobCallback::OnJobComplete()
   */
  virtual bool DoWork() = 0; // function to do the work

  /*!
   \brief Function that returns the type of job.

   CJob subclasses may optionally implement this function to specify the type of job.
   This is useful for the CJobManager::AddLIFOJob() routine, which preempts similar jobs
   with the new job.

   \return a unique character string describing the job.
   \sa CJobManager
   */
  virtual const char* GetType() const = 0;

  /*!
   \brief Function for longer jobs to report progress and check whether they have been cancelled.

   Jobs that contain loops that may take time should check this routine each iteration of the loop,
   both to (optionally) report progress, and to check for cancellation.

   \param progress the amount of the job performed, out of total.
   \param total the total amount of processing to be performed
   \return if true, the job has been asked to cancel.

   \sa IJobCallback::OnJobProgress()
   */
  virtual bool ShouldCancel(unsigned int progress, unsigned int total) const = 0;

  virtual void CancelJob() = 0;

  virtual void FreeJob() = 0;

  virtual void SetJobManager(CJobManager* manager) = 0;

  virtual void SetCallback(IJobCallback* callback) = 0;
  virtual IJobCallback* GetCallback() const = 0;

  virtual void SetJobID(const uint32_t id) = 0;
  virtual uint32_t GetJobID() const = 0;

  virtual void SetPriority(const JobPriority& priority) = 0;
  virtual JobPriority GetPriority() const = 0;
};
