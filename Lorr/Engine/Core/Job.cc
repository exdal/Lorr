// Created on Wednesday May 17th 2023 by exdal
// Last modified on Monday June 12th 2023 by exdal

#include "Job.hh"
#include "Utils/Timer.hh"

#include <x86intrin.h>

namespace lr::Job
{
static JobManager _man;

static iptr WorkerFn(void *pData)
{
    Worker *pThis = (Worker *)pData;

    while (true)
    {
        if (_man.JobAvailable())
        {
            ZoneScoped;

            usize jobID = _man.PopJob();
            _man.m_pJobs[jobID]();

            continue;
        }

        EAProcessorPause();
    }

    return 1;
}

void Worker::Init(u32 id, u32 processor)
{
    ZoneScoped;

    m_ID = id;
    m_Processor = processor;

    eastl::string threadName = _FMT("WRKR{}", id);
    EA::Thread::ThreadParameters threadParams = {};
    threadParams.mbDisablePriorityBoost = false;
    threadParams.mnProcessor = processor;
    threadParams.mpName = threadName.c_str();

    m_Thread.Begin(WorkerFn, this, &threadParams);
}

void JobManager::Init(u32 threadCount)
{
    ZoneScoped;

    LOG_TRACE("Initializing {} worker threads.", threadCount);
    for (u32 i = 0; i < threadCount; i++)
    {
        Worker *pWorker = (Worker *)_man.m_Workers.push_back_uninitialized();
        pWorker->Init(i, i);
    }
}

void JobManager::Schedule(JobFn fn)
{
    ZoneScoped;

    u64 mask = _man.m_JobsMask.load(eastl::memory_order_acquire);
    u64 i = _tzcnt_u64(~mask);
    _man.m_pJobs[i] = fn;
    mask |= 1 << i;
    _man.m_JobsMask.store(mask, eastl::memory_order_release);
}

usize JobManager::PopJob()
{
    u64 mask = _man.m_JobsMask.load(eastl::memory_order_acquire);
    usize jobID = _tzcnt_u64(mask);
    mask &= ~(1 << jobID);
    _man.m_JobsMask.store(mask, eastl::memory_order_release);

    return jobID;
}

void JobManager::WaitForAll()
{
    while (_man.m_JobsMask.load(eastl::memory_order_relaxed) != 0)
        ;
}

bool JobManager::JobAvailable()
{
    return __builtin_popcountll(_man.m_JobsMask.load(eastl::memory_order_relaxed));
}

JobManager &JobManager::Get()
{
    ZoneScoped;

    return _man;
}

}  // namespace lr::Job