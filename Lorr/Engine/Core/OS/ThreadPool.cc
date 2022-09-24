#include "ThreadPool.hh"

namespace lr
{
    struct WorkerUserData
    {
        ThreadPoolSPSC *pPool = nullptr;
        u32 Index = -1;
    };

    intptr_t ThreadPoolSPSC::WorkerFunction(void *pData)
    {
        ZoneScoped;
        
        WorkerUserData *pUserData = (WorkerUserData *)pData;
        ThreadPoolSPSC *pPool = pUserData->pPool;
        u32 index = pUserData->Index;

        ThreadPoolSPSC::WorkerData &worker = pPool->m_Workers[index];

        while (!pPool->m_Shutdown)
        {
            ThreadPoolFn *pJob = worker.pJob.load(eastl::memory_order_acquire);

            if (pJob == nullptr)
            {
                EAProcessorPause();
                continue;
            }

            ThreadPoolFn &func = *pJob;
            func();

            while (!worker.pJob.compare_exchange_weak(pJob, nullptr, eastl::memory_order_release))
            {
                pJob = nullptr;
            }
        }

        delete pUserData;

        return 0;
    }

    void ThreadPoolSPSC::Init()
    {
        ZoneScoped;
        
        m_Shutdown = false;

        u32 processorCount = 1;
        m_ThreadCount = processorCount;
        m_AffinityMask = 0xffffffff << processorCount ^ 0xffffffff;

        LOG_TRACE("ThreadPoolSWSC available processors = {}, affinity mask = 0x{:X}", processorCount, m_AffinityMask);

        for (u32 i = 0; i < processorCount; i++)
        {
            EA::Thread::ThreadParameters threadParams;
            threadParams.mnAffinityMask = 1 << (i % processorCount);
            threadParams.mnProcessor = EA::Thread::kProcessorAny;

            WorkerUserData *pUd = new WorkerUserData;
            pUd->pPool = this;
            pUd->Index = i;

            m_Workers[i].Thread.Begin(WorkerFunction, pUd, &threadParams);
            m_Workers[i].Busy = false;
        }
    }

    u32 ThreadPoolSPSC::Acquire()
    {
        ZoneScoped;
        
        if (m_AffinityMask == 0)
        {
            LOG_TRACE("All workers are in use, we have to wait for one to become available.");
            return -1;
        }

        u32 setBitPosition = 0;
        // Get available bit and return
        setBitPosition = log2(m_AffinityMask & -m_AffinityMask);

        // Set that bit to 0 and store, so we make it "in use"
        m_AffinityMask ^= 1 << setBitPosition;

        return setBitPosition;
    }

    void ThreadPoolSPSC::Release(u32 id)
    {
        ZoneScoped;
        
        m_AffinityMask |= (1 << id);
    }

    void ThreadPoolSPSC::WaitFor(u32 id)
    {
        ZoneScoped;
        
        while (m_Workers[id].pJob.load(eastl::memory_order_acquire))
        {
            EAProcessorPause();
        }
    }

    void ThreadPoolSPSC::PushJob(u32 id, ThreadPoolFn func)
    {
        ZoneScoped;

        WorkerData &worker = m_Workers[id];
        worker.pJob.store(&func, eastl::memory_order_release);
    }

}  // namespace lr