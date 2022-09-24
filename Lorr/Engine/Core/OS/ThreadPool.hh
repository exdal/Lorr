//
// Created on Saturday 28th May 2022 by e-erdal
//

#pragma once

#include <eathread/eathread_thread.h>

namespace lr
{
    using ThreadPoolFn = eastl::function<void()>;

    // Implementation of Single Producer Single Writer thread pool
    class ThreadPoolSPSC
    {
    public:
        ThreadPoolSPSC() = default;

        void Init();

        u32 Acquire();
        void Release(u32 id);
        void WaitFor(u32 id);
        void PushJob(u32 id, ThreadPoolFn func);

    private:
        bool m_Shutdown;
        u32 m_AffinityMask;
        u32 m_ThreadCount = 0;

        struct WorkerData
        {
            eastl::atomic<bool> Busy;
            EA::Thread::Thread Thread;
            eastl::atomic<ThreadPoolFn *> pJob = nullptr;
        };

        eastl::array<WorkerData, 32> m_Workers;

        static intptr_t WorkerFunction(void *pData);
    };

}  // namespace lr