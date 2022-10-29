#include "log.h"
#include "threads/IThreadImpl.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string_view>
#include <thread>

#include <experimental/map>

#pragma once

enum class JobPriorityNew
{
  PAUSABLE,
  LOW,
  MEDIUM,
  HIGH,
  DEDICATED
};

enum class JobQueueMode
{
  FIFO,
  LIFO,
};

using namespace std::chrono_literals;

using JobID = uint32_t;
using QueueID = uint32_t;
class IJobNew
{
public:
  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual bool Finished() = 0;

  virtual bool ShouldCancel(unsigned int progress, unsigned int total) const = 0;

protected:
  IJobNew() = default;
  virtual ~IJobNew() = default;

  virtual void Process() = 0;
};

class CJobManagerNew;

class CJobNew;

class IJobCallBackNew
{
public:
  virtual void OnJobCompleted(JobID id, bool success, CJobNew* job) = 0;
  virtual bool OnJobProgress(JobID id, int progress, int total, const CJobNew* job) = 0;

protected:
  IJobCallBackNew() = default;
  virtual ~IJobCallBackNew() = default;
};

class CJobTimer
{
public:
  void Queued() { m_queued = std::chrono::steady_clock::now(); }

  void Processed() { m_processed = std::chrono::steady_clock::now(); }

protected:
  explicit CJobTimer(JobID id) : m_id(id), m_created(std::chrono::steady_clock::now()) {}
  ~CJobTimer()
  {
    m_deleted = std::chrono::steady_clock::now();

    const auto queueing = m_queued - m_created;
    const auto processing = m_processed - m_queued;
    const auto finalizing = m_deleted - m_processed;

    CLog::Log(LOGDEBUG, "timing for job id: {}", m_id);
    CLog::Log(
        LOGDEBUG, "  queuing:    {:.3f}ms",
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(queueing).count());
    CLog::Log(
        LOGDEBUG, "  processing: {:.3f}ms",
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(processing).count());
    CLog::Log(
        LOGDEBUG, "  finalizing: {:.3f}ms",
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(finalizing).count());
  }

private:
  JobID m_id;
  std::chrono::steady_clock::time_point m_created;
  std::chrono::steady_clock::time_point m_queued;
  std::chrono::steady_clock::time_point m_processed;
  std::chrono::steady_clock::time_point m_deleted;
};

class CJobNew : public CJobTimer, public IJobNew
{
public:
  CJobNew(JobID id, std::string_view name, IJobCallBackNew* callback);
  virtual ~CJobNew() override { Stop(); };

  void Start() override
  {
    Queued();
    CLog::Log(LOGINFO, "Starting Job: {} id: {}", m_name, m_id);

    m_future = m_promise.get_future();

    m_futureThread = m_promiseThread.get_future();

    m_runner = std::thread(&CJobNew::Process, this);

    m_futureThread.get();

    auto impl = IThreadImpl::CreateThreadImpl(m_runner.native_handle());
    impl->SetThreadInfo(std::string(m_name));
  }

  void Stop() override;

  bool Finished() override
  {
    std::future_status status = m_future.wait_for(0ms);

    return status == std::future_status::ready;
  }

  void Process() override;

  virtual bool ShouldCancel(unsigned int progress, unsigned int total) const override
  {
    if (m_callback)
      m_callback->OnJobProgress(m_id, progress, total, this);

    return false;
  }

protected:
  JobID m_id;
  std::string_view m_name;
  std::promise<void> m_promise;
  std::future<void> m_future;

  std::promise<void> m_promiseThread;
  std::future<void> m_futureThread;

private:
  virtual bool DoWork() = 0;

  std::atomic<bool> m_stop;

  IJobCallBackNew* m_callback;

  std::thread m_runner;
};

class CJobQueueNew : public IJobCallBackNew
{
public:
  CJobQueueNew(JobPriorityNew priority, uint32_t jobsAtOnce, JobQueueMode mode);
  virtual ~CJobQueueNew() override;

  template<typename T, typename... Args>
  void Submit(std::string_view name, Args&&... args);

  uint32_t GetJobsAtOnce() const { return m_jobsAtOnce; }

  JobPriorityNew GetJobPriority() const { return m_priority; }

private:
  JobPriorityNew m_priority;
  uint32_t m_jobsAtOnce;
  JobQueueMode m_mode;
  // QueueID m_id;
};

template<typename F>
class CLambdaJobNew;

class CJobManagerNew
{
public:
  static CJobManagerNew& Get();

  void Stop(JobID id)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_jobs[id]->Stop();
  }

  void Notify() { m_condition.notify_one(); }

  void RemoveQueue(CJobQueueNew* queue)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_queues.erase(queue);
  }

  void AddQueue(CJobQueueNew* queue)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_queues.emplace(queue, std::deque<std::pair<JobID, std::unique_ptr<CJobNew>>>());
  }

  template<typename T, typename... Args>
  void Submit(CJobQueueNew* queue, JobQueueMode mode, Args&&... args)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    JobID id = m_counter++;

    auto job = std::make_unique<T>(id, std::forward<Args>(args)...);

    auto pair = std::pair<JobID, std::unique_ptr<CJobNew>>(id, std::move(job));

    if (mode == JobQueueMode::FIFO)
      m_queues[queue].emplace_back(std::move(pair));

    if (mode == JobQueueMode::LIFO)
      m_queues[queue].emplace_front(std::move(pair));

    m_condition.notify_one();
  }

  template<typename F, typename... Args>
  JobID Submit(JobPriorityNew priority,
               std::string_view name,
               IJobCallBackNew* callback,
               F&& f,
               Args&&... args)
  {
    return Submit<CLambdaJobNew<F>>(priority, name, callback, std::forward<F>(f),
                                    std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
  JobID Submit(JobPriorityNew priority, Args&&... args)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    JobID id = m_counter++;

    auto job = std::make_unique<T>(id, std::forward<Args>(args)...);

    auto pair = std::pair<JobID, std::unique_ptr<CJobNew>>(id, std::move(job));

    const auto& it = m_queue[priority].emplace(std::move(pair));

    m_condition.notify_one();

    return it.first;
  }

  void Process()
  {
    while (!m_stop)
    {
      std::unique_lock<std::mutex> lock(m_mutex);

      // CLog::Log(LOGINFO, "JobManagerNew Processing");

      // CLog::Log(LOGINFO, "JobManagerNew size: {}", m_jobs.size());

      std::experimental::erase_if(m_jobs, [](const auto& job) { return job.second->Finished(); });

      // auto it = m_jobs.begin();

      // for (; it != m_jobs.end();)
      // {
      //   auto& job = (*it).second;
      //   if (job->Finished())
      //   {
      //     it = m_jobs.erase(it);
      //   }
      //   else
      //   {
      //     it++;
      //   }
      // }

      for (auto& it : m_queues)
      {
        for (uint32_t count = 0; count < it.first->GetJobsAtOnce(); count++)
        {
          auto job = std::move(it.second.front());
          it.second.pop_front();

          m_queue[it.first->GetJobPriority()].emplace(std::move(job));
        }
      }

      for (auto& it : m_queue)
      {
        if (it.second.empty())
          continue;

        // CLog::Log(LOGINFO, "JobManagerNew priority {} has jobs: {}", static_cast<int>(it.first), it.second.size());
        if (it.first == JobPriorityNew::PAUSABLE && m_paused)
          continue;

        if ((it.first == JobPriorityNew::DEDICATED && m_jobs.size() < 1000) ||
            (it.first == JobPriorityNew::HIGH && m_jobs.size() < 4) ||
            (it.first == JobPriorityNew::MEDIUM && m_jobs.size() < 3) ||
            (it.first == JobPriorityNew::LOW && m_jobs.size() < 2) ||
            (it.first == JobPriorityNew::PAUSABLE && m_jobs.size() < 2))
        {
          auto job = std::move(it.second.front());
          it.second.pop();

          CLog::Log(LOGINFO, "JobManagerNew adding job with id: {}", job.first);

          auto [it, success] = m_jobs.emplace(std::move(job));

          if (!success)
            throw std::runtime_error("failed to add job");

          (*it).second->Start();
        }
      }

      m_condition.wait_for(lock, 5s,
                           [this]()
                           {
                             if (m_stop)
                               return true;

                             for (auto& it : m_queue)
                             {
                               if (!it.second.empty())
                                 return true;
                             }

                             for (auto& it : m_jobs)
                             {
                               if (it.second->Finished())
                                 return true;
                             }

                             return false;
                           });
    }
  }

  void Start() { m_thread = std::thread(&CJobManagerNew::Process, this); }

  void Stop()
  {
    m_jobs.clear();

    m_stop = true;

    m_condition.notify_one();

    if (m_thread.joinable())
      m_thread.join();
  }

  void Pause() { m_paused = true; }
  void UnPause() { m_paused = false; }

protected:
  CJobManagerNew() = default;
  ~CJobManagerNew() { Stop(); }

private:
  JobID m_counter{1};
  QueueID m_queueCounter{1};

  std::map<CJobQueueNew*, std::deque<std::pair<JobID, std::unique_ptr<CJobNew>>>> m_queues;

  std::map<JobPriorityNew, std::queue<std::pair<JobID, std::unique_ptr<CJobNew>>>> m_queue;
  std::map<JobID, std::unique_ptr<CJobNew>> m_jobs;

  std::mutex m_mutex;

  bool m_paused{false};

  std::atomic<bool> m_stop{false};

  std::thread m_thread;

  std::condition_variable m_condition;
};

template<typename F>
class CLambdaJobNew : public CJobNew
{
public:
  CLambdaJobNew(JobID id, std::string_view name, IJobCallBackNew* callback, F&& f)
    : CJobNew(id, name, callback), m_f(std::forward<F>(f))
  {
  }
  ~CLambdaJobNew() override = default;

protected:
  void Process() override
  {
    m_promiseThread.set_value();

    CLog::Log(LOGINFO, "Processing Job: {} id: {}", m_name, m_id);

    m_f();

    Processed();

    m_promise.set_value_at_thread_exit();

    CJobManagerNew::Get().Notify();
  }

private:
  virtual bool DoWork() { return true; }

  F m_f;
};

template<typename T, typename... Args>
void CJobQueueNew::Submit(std::string_view name, Args&&... args)
{
  CJobManagerNew::Get().Submit<T>(this, m_mode, name, std::forward<Args>(args)...);
}
