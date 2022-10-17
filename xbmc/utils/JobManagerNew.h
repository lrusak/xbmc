
#include "log.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <thread>

enum class JobPriorityNew
{
  PAUSABLE,
  LOW,
  MEDIUM,
  HIGH,
  DEDICATED
};

using namespace std::chrono_literals;

using JobID = uint32_t;

class IJobNew
{
protected:
  IJobNew() = default;
  virtual ~IJobNew() = default;

  virtual void Process() = 0;
};

class CJobManagerNew;

class CJobNew : public IJobNew
{
public:
  CJobNew(JobID id, CJobManagerNew* manager, std::string_view name)
    : IJobNew(), m_id(id), m_manager(manager), m_name(name), m_stop(false)
  {
  }
  virtual ~CJobNew() override { Stop(); }

  void Start()
  {
    CLog::Log(LOGINFO, "Starting Job: {} id: {}", m_name, m_id);

    m_runner = std::thread(&CJobNew::Process, this);
  }

  void Stop()
  {
    CLog::Log(LOGINFO, "Job Stopping: {} id: {}", m_name, m_id);

    m_stop = true;

    if (m_runner.joinable())
      m_runner.join();

    CLog::Log(LOGINFO, "Job Complete: {} id: {}", m_name, m_id);
  }

protected:
  virtual void Process() override
  {
    CLog::Log(LOGINFO, "Processing Job: {} id: {}", m_name, m_id);

    while (!m_stop)
    {
      CLog::Log(LOGINFO, "Running: {} id: {}", m_name, m_id);
      std::this_thread::sleep_for(100ms);
    }
  }

private:
  JobID m_id;
  CJobManagerNew* m_manager;
  std::string_view m_name;

  std::atomic<bool> m_stop;

  std::thread m_runner;
};

class CJobManagerNew
{
public:
  static CJobManagerNew& Get();

  void Stop(JobID id)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_jobs[id]->Stop();

    m_jobs.erase(id);
  }

  template<typename T, typename... Args>
  JobID Submit(JobPriorityNew priority, Args&&... args)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    JobID id = m_counter++;

    auto job = std::make_unique<T>(id, this, std::forward<Args>(args)...);

    auto pair = std::pair<JobID, std::unique_ptr<CJobNew>>(id, std::move(job));

    const auto& it = m_queue[priority].emplace(std::move(pair));

    return it.first;
  }

  void Process()
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    CLog::Log(LOGINFO, "JobManagerNew Processing");

    if (m_jobs.size() == 3)
      return;

    CLog::Log(LOGINFO, "JobManagerNew size: {}", m_jobs.size());

    for (auto& it : m_queue)
    {
      if (it.second.empty())
        continue;

      CLog::Log(LOGINFO, "JobManagerNew priority {} has jobs", static_cast<int>(it.first));
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

      if (m_jobs.size() == 3)
        return;
    }
  }

  void Stop() { m_jobs.clear(); }

  void Pause() { m_paused = true; }
  void UnPause() { m_paused = false; }

protected:
  CJobManagerNew() = default;
  ~CJobManagerNew() = default;

private:
  JobID m_counter;

  std::map<JobPriorityNew, std::queue<std::pair<JobID, std::unique_ptr<CJobNew>>>> m_queue;
  std::map<JobID, std::unique_ptr<CJobNew>> m_jobs;

  std::mutex m_mutex;

  bool m_paused{false};
};
