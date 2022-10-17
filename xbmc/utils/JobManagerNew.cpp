
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
