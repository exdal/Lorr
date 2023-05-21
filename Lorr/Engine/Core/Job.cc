// Created on Wednesday May 17th 2023 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#include "Job.hh"
#include "Utils/Timer.hh"

namespace lr::Job
{
static JobManager _man;

static iptr WorkerFn(void *pData)
{
    Worker *pThis = (Worker *)pData;
    fmtlog::setThreadName(Format("WRKR{}", pThis->m_ID).c_str());

    while (true)
    {
        JobFn func;
        if (_man.m_Jobs.pop(func))
        {
            pThis->SetBusy();
            func();

            continue;
        }

        pThis->SetIdle();

        EAProcessorPause();  // maximize the throughput
    }

    return 1;
}

void Worker::Init(u32 id, u32 processor)
{
    ZoneScoped;

    m_ID = id;
    m_Processor = processor;

    EA::Thread::ThreadParameters threadParams = {};
    threadParams.mbDisablePriorityBoost = false;
    threadParams.mnProcessor = processor;
    threadParams.mpName = Format("WRKR{}", id).c_str();

    m_Thread.Begin(WorkerFn, this, &threadParams);
}

void Worker::SetBusy()
{
    ZoneScoped;

    u64 mask = _man.m_StatusMask.load(eastl::memory_order_acquire);
    mask |= 1 << m_ID;
    _man.m_StatusMask.store(mask, eastl::memory_order_release);
}

void Worker::SetIdle()
{
    ZoneScoped;

    u64 mask = _man.m_StatusMask.load(eastl::memory_order_acquire);
    mask &= ~(1 << m_ID);
    _man.m_StatusMask.store(mask, eastl::memory_order_release);
}

void JobManager::Init(u32 threadCount)
{
    ZoneScoped;

    for (u32 i = 0; i < threadCount; i++)
    {
        Worker *pWorker = (Worker *)_man.m_Workers.push_back_uninitialized();
        pWorker->Init(i, i);
    }
}

void JobManager::Schedule(JobFn fn)
{
    ZoneScoped;

    _man.m_Jobs.push(fn);
}

void JobManager::WaitForAll()
{
    u64 mask = _man.m_StatusMask.load(eastl::memory_order_acquire);
    while (_man.m_StatusMask.load(eastl::memory_order_acquire) != 0 || !_man.m_Jobs.empty())
        ;
}

JobManager &JobManager::Get()
{
    ZoneScoped;

    return _man;
}

}  // namespace lr::Job