
#include "JobManagerNew.h"

CJobManagerNew& CJobManagerNew::Get()
{
  static CJobManagerNew jobManagerNew;

  return jobManagerNew;
}

// CJobManagerNew::~CJobManagerNew()
// {
//   for (auto& job : m_jobs)
//   {
//     job.second->Stop();
//   }
// }

CJobNew::CJobNew(JobID id, std::string_view name, IJobCallBackNew* callback)
  : CJobTimer(id), IJobNew(), m_id(id), m_name(name), m_stop(false), m_callback(callback)
{
}

void CJobNew::Stop()
{
  CLog::Log(LOGINFO, "Job Stopping: {} id: {}", m_name, m_id);

  m_stop = true;

  if (m_runner.joinable())
    m_runner.join();

  CLog::Log(LOGINFO, "Job Complete: {} id: {}", m_name, m_id);
}

void CJobNew::Process()
{
  m_promiseThread.set_value();

  CLog::Log(LOGINFO, "Processing Job: {} id: {}", m_name, m_id);

  bool success = DoWork();

  Processed();

  if (m_callback)
    m_callback->OnJobCompleted(m_id, success, this);

  m_promise.set_value_at_thread_exit();

  CJobManagerNew::Get().Notify();
}

CJobQueueNew::CJobQueueNew(JobPriorityNew priority, uint32_t jobsAtOnce, JobQueueMode mode)
  : m_priority(priority), m_jobsAtOnce(jobsAtOnce), m_mode(mode)
{
  CJobManagerNew::Get().AddQueue(this);
}

CJobQueueNew::~CJobQueueNew()
{
  CJobManagerNew::Get().RemoveQueue(this);
}
