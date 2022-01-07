/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JobManager.h"

#include "threads/SingleLock.h"
#include "utils/XTimeUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <stdexcept>

using namespace std::chrono_literals;

namespace
{

struct JobPriorityStruct
{
  JobPriority priority;
  std::string_view priorityName;
  size_t workers;
};

constexpr std::array<JobPriorityStruct, 5> jobPriorityMap = {{
    {JobPriority::LOW_PAUSABLE, "low pausable", 2},
    {JobPriority::LOW, "low", 3},
    {JobPriority::NORMAL, "normal", 4},
    {JobPriority::HIGH, "high", 5},
    {JobPriority::DEDICATED, "dedicated", 10000},
}};

size_t JobPriorityToWorkerCount(const JobPriority& priority)
{
  auto it = std::find_if(jobPriorityMap.cbegin(), jobPriorityMap.cend(),
                         [&priority](const JobPriorityStruct& priorityStruct)
                         { return priorityStruct.priority == priority; });
  if (it != jobPriorityMap.cend())
    return it->workers;

  throw std::runtime_error("job priority not found");
}

//! @todo: c++20 constexpr std::find_if
std::string_view JobPriorityToPriorityString(const JobPriority& priority)
{
  auto it = std::find_if(jobPriorityMap.cbegin(), jobPriorityMap.cend(),
                         [&priority](const JobPriorityStruct& priorityStruct)
                         { return priorityStruct.priority == priority; });
  if (it != jobPriorityMap.cend())
    return it->priorityName;

  throw std::runtime_error("job priority not found");
}
}

CJobWorker::CJobWorker(CJobManager *manager) : CThread("JobWorker")
{
  m_jobManager = manager;
  Create(true); // start work immediately, and kill ourselves when we're done
}

CJobWorker::~CJobWorker()
{
  // while we should already be removed from the job manager, if an exception
  // occurs during processing that we haven't caught, we may skip over that step.
  // Thus, before we go out of scope, ensure the job manager knows we're gone.
  m_jobManager->RemoveWorker(this);
  if(!IsAutoDelete())
    StopThread();
}

void CJobWorker::Process()
{
  SetPriority( GetMinPriority() );
  while (true)
  {
    // request an item from our manager (this call is blocking)
    IJob* job = m_jobManager->GetNextJob(this);
    if (!job)
      break;

    bool success = false;
    try
    {
      success = job->DoWork();
    }
    catch (...)
    {
      CLog::Log(LOGERROR, "{} error processing job {}", __FUNCTION__, job->GetType());
    }
    m_jobManager->OnJobComplete(success, job);
  }
}

CJobQueue::CJobQueue(bool lifo, unsigned int jobsAtOnce, JobPriority priority)
  : m_jobsAtOnce(jobsAtOnce), m_priority(priority), m_lifo(lifo)
{
}

CJobQueue::~CJobQueue()
{
  CancelJobs();
}

void CJobQueue::OnJobComplete(unsigned int jobID, bool success, IJob* job)
{
  OnJobNotify(job);
}

void CJobQueue::OnJobAbort(unsigned int jobID, IJob* job)
{
  OnJobNotify(job);
}

void CJobQueue::CancelJob(const IJob* job)
{
  CSingleLock lock(m_section);
  Processing::iterator i = find(m_processing.begin(), m_processing.end(), job);
  if (i != m_processing.end())
  {
    (*i)->CancelJob();
    m_processing.erase(i);
    return;
  }
  Queue::iterator j = find(m_jobQueue.begin(), m_jobQueue.end(), job);
  if (j != m_jobQueue.end())
  {
    (*j)->FreeJob();
    m_jobQueue.erase(j);
  }
}

bool CJobQueue::AddJob(IJob* job)
{
  CSingleLock lock(m_section);
  // check if we have this job already.  If so, we're done.
  if (find(m_jobQueue.begin(), m_jobQueue.end(), job) != m_jobQueue.end() ||
      find(m_processing.begin(), m_processing.end(), job) != m_processing.end())
  {
    delete job;
    return false;
  }

  if (m_lifo)
    m_jobQueue.push_back(job);
  else
    m_jobQueue.push_front(job);
  QueueNextJob();

  return true;
}

void CJobQueue::OnJobNotify(IJob* job)
{
  CSingleLock lock(m_section);

  // check if this job is in our processing list
  const auto it = std::find(m_processing.begin(), m_processing.end(), job);
  if (it != m_processing.end())
    m_processing.erase(it);
  // request a new job be queued
  QueueNextJob();
}

void CJobQueue::QueueNextJob()
{
  CSingleLock lock(m_section);
  while (m_jobQueue.size() && m_processing.size() < m_jobsAtOnce)
  {
    IJob* job = m_jobQueue.back();
    job->SetJobID(CJobManager::GetInstance().AddJob(job, this, m_priority));
    if (job->GetJobID() > 0)
    {
      m_processing.emplace_back(job);
      m_jobQueue.pop_back();
      return;
    }
    m_jobQueue.pop_back();
  }
}

void CJobQueue::CancelJobs()
{
  CSingleLock lock(m_section);
  for_each(m_processing.begin(), m_processing.end(), [](IJob* jp) { jp->CancelJob(); });
  for_each(m_jobQueue.begin(), m_jobQueue.end(), [](IJob* jp) { jp->FreeJob(); });
  m_jobQueue.clear();
  m_processing.clear();
}

bool CJobQueue::IsProcessing() const
{
  return CJobManager::GetInstance().m_running && (!m_processing.empty() || !m_jobQueue.empty());
}

bool CJobQueue::QueueEmpty() const
{
  CSingleLock lock(m_section);
  return m_jobQueue.empty();
}

CJobManager &CJobManager::GetInstance()
{
  static CJobManager sJobManager;
  return sJobManager;
}

CJobManager::CJobManager()
{
  m_jobCounter = 0;
  m_running = true;
  m_pauseJobs = false;
}

void CJobManager::Restart()
{
  CSingleLock lock(m_section);

  if (m_running)
    throw std::logic_error("CJobManager already running");
  m_running = true;
}

void CJobManager::CancelJobs()
{
  CSingleLock lock(m_section);
  m_running = false;

  // clear any pending jobs
  for (const auto& priorityStruct : jobPriorityMap)
  {
    const JobPriority& priority = priorityStruct.priority;

    std::for_each(m_jobQueue[priority].begin(), m_jobQueue[priority].end(),
                  [](IJob* wi)
                  {
                    if (wi)
                    {
                      IJobCallback* callback = wi->GetCallback();
                      if (callback)
                        callback->OnJobAbort(wi->GetJobID(), wi);
                      wi->FreeJob();
                    }
                  });
    m_jobQueue[priority].clear();
  }

  // cancel any callbacks on jobs still processing
  std::for_each(m_processing.begin(), m_processing.end(),
                [](IJob* wi)
                {
                  if (wi)
                  {
                    IJobCallback* callback = wi->GetCallback();
                    if (callback)
                      callback->OnJobAbort(wi->GetJobID(), wi);
                    wi->CancelJob();
                  }
                });

  // tell our workers to finish
  while (m_workers.size())
  {
    lock.Leave();
    m_jobEvent.Set();
    std::this_thread::yield(); // yield after setting the event to give the workers some time to die
    lock.Enter();
  }
}

unsigned int CJobManager::AddJob(IJob* job, IJobCallback* callback, JobPriority priority)
{
  CSingleLock lock(m_section);

  if (!m_running)
  {
    delete job;
    return 0;
  }

  // increment the job counter, ensuring 0 (invalid job) is never hit
  m_jobCounter++;
  if (m_jobCounter == 0)
    m_jobCounter++;

  // create a work item for this job
  job->SetJobID(m_jobCounter);
  job->SetPriority(priority);
  job->SetCallback(callback);

  m_jobQueue[priority].push_back(job);

  CLog::Log(LOGDEBUG, "[JOBMANAGER] adding job: {} id: {} priority: {} type: {}", fmt::ptr(job),
            job->GetJobID(), JobPriorityToPriorityString(job->GetPriority()), job->GetType());

  StartWorkers(priority);
  return job->GetJobID();
}

void CJobManager::CancelJob(unsigned int jobID)
{
  CSingleLock lock(m_section);

  // check whether we have this job in the queue
  for (const auto& priorityStruct : jobPriorityMap)
  {
    const JobPriority& priority = priorityStruct.priority;

    auto queue = &m_jobQueue[priority];

    queue->erase(std::remove_if(queue->begin(), queue->end(),
                                [&jobID](auto q)
                                {
                                  if (q->GetJobID() == jobID)
                                  {
                                    delete q;
                                    return true;
                                  }

                                  return false;
                                }),
                 queue->end());
  }

  // or if we're processing it
  std::for_each(m_processing.begin(), m_processing.end(),
                [&jobID](auto& job)
                {
                  // job is in progress, so only thing to do is to remove callback
                  if (job->GetJobID() == jobID)
                    job->SetCallback(nullptr);
                });
}

void CJobManager::StartWorkers(JobPriority priority)
{
  CSingleLock lock(m_section);

  // check how many free threads we have
  if (m_processing.size() >= JobPriorityToWorkerCount(priority))
    return;

  // do we have any sleeping threads?
  if (m_processing.size() < m_workers.size())
  {
    m_jobEvent.Set();
    return;
  }

  // everyone is busy - we need more workers
  m_workers.push_back(new CJobWorker(this));
}

IJob* CJobManager::PopJob()
{
  CSingleLock lock(m_section);
  for (const auto& priorityStruct : jobPriorityMap)
  {
    const JobPriority& priority = priorityStruct.priority;

    // Check whether we're pausing pausable jobs
    if (priority == JobPriority::LOW_PAUSABLE && m_pauseJobs)
      continue;

    if (m_jobQueue[priority].size() && m_processing.size() < JobPriorityToWorkerCount(priority))
    {
      // pop the job off the queue
      IJob* job = m_jobQueue[priority].front();
      m_jobQueue[priority].pop_front();

      // add to the processing vector
      m_processing.push_back(job);

      job->SetJobManager(this);

      return job;
    }
  }

  return nullptr;
}

void CJobManager::PauseJobs()
{
  CSingleLock lock(m_section);
  m_pauseJobs = true;
}

void CJobManager::UnPauseJobs()
{
  CSingleLock lock(m_section);
  m_pauseJobs = false;
}

bool CJobManager::IsProcessing(const JobPriority& priority) const
{
  CSingleLock lock(m_section);

  if (m_pauseJobs)
    return false;

  for(Processing::const_iterator it = m_processing.begin(); it < m_processing.end(); ++it)
  {
    if (priority == (*it)->GetPriority())
      return true;
  }
  return false;
}

int CJobManager::IsProcessing(const std::string &type) const
{
  int jobsMatched = 0;
  CSingleLock lock(m_section);

  if (m_pauseJobs)
    return 0;

  for(Processing::const_iterator it = m_processing.begin(); it < m_processing.end(); ++it)
  {
    if (type == std::string((*it)->GetType()))
      jobsMatched++;
  }
  return jobsMatched;
}

IJob* CJobManager::GetNextJob(const CJobWorker* worker)
{
  CSingleLock lock(m_section);
  while (m_running)
  {
    // grab a job off the queue if we have one
    IJob* job = PopJob();
    if (job)
      return job;
    // no jobs are left - sleep for 30 seconds to allow new jobs to come in
    lock.Leave();
    bool newJob = m_jobEvent.Wait(30000ms);
    lock.Enter();
    if (!newJob)
      break;
  }
  // ensure no jobs have come in during the period after
  // timeout and before we held the lock
  IJob* job = PopJob();
  if (job)
    return job;
  // have no jobs
  RemoveWorker(worker);
  return NULL;
}

bool CJobManager::OnJobProgress(unsigned int progress, unsigned int total, const IJob* job) const
{
  CSingleLock lock(m_section);
  // find the job in the processing queue, and check whether it's cancelled (no callback)
  Processing::const_iterator i = find(m_processing.begin(), m_processing.end(), job);
  if (i != m_processing.end())
  {
    lock.Leave(); // leave section prior to call
    IJobCallback* callback = (*i)->GetCallback();

    if (callback)
    {
      callback->OnJobProgress((*i)->GetJobID(), progress, total, job);
      return false;
    }
  }
  return true; // couldn't find the job, or it's been cancelled
}

void CJobManager::OnJobComplete(bool success, IJob* job)
{
  CSingleLock lock(m_section);
  // remove the job from the processing queue
  Processing::iterator i = find(m_processing.begin(), m_processing.end(), job);
  if (i != m_processing.end())
  {
    // tell any listeners we're done with the job, then delete it
    lock.Leave();
    try
    {
      IJobCallback* callback = (*i)->GetCallback();
      CLog::Log(LOGDEBUG, "OnJobComplete: {} id: {}", fmt::ptr(job), (*i)->GetJobID());
      if ((*i) && callback && (*i)->GetJobID() != 0)
        callback->OnJobComplete((*i)->GetJobID(), success, job);
    }
    catch (...)
    {
      CLog::Log(LOGERROR, "{} error processing job {}", __FUNCTION__, (*i)->GetType());
    }
    lock.Enter();
    m_processing.erase(i);
    lock.Leave();
    job->FreeJob();
  }
}

void CJobManager::RemoveWorker(const CJobWorker *worker)
{
  CSingleLock lock(m_section);
  // remove our worker
  Workers::iterator i = find(m_workers.begin(), m_workers.end(), worker);
  if (i != m_workers.end())
    m_workers.erase(i); // workers auto-delete
}

unsigned int CJobManager::GetMaxWorkers(JobPriority priority)
{


  static const unsigned int max_workers = 5;
  if (priority == JobPriority::DEDICATED)
    return 10000; // A large number..

  return max_workers - (static_cast<int>(JobPriority::HIGH) - static_cast<int>(priority));
}
