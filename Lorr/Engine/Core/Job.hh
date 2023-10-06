// Created on Tuesday May 16th 2023 by exdal
// Last modified on Monday June 12th 2023 by exdal

#pragma once

#include <EASTL/atomic.h>
#include <EASTL/fixed_vector.h>
#include <eathread/eathread_thread.h>

namespace lr::Job
{
using JobFn = eastl::function<void()>;

struct Worker
{
    void Init(u32 id, u32 processor);

    u32 m_ID = 0;
    u32 m_Processor = 0;
    EA::Thread::Thread m_Thread;
};

struct JobManager
{
    static void Init(u32 threadCount);
    static void Schedule(JobFn fn);
    static usize PopJob();
    static void WaitForAll();
    static bool JobAvailable();
    static void SetBusy(Worker *pWorker);
    static void SetIdle(Worker *pWorker);
    static JobManager &Get();

    eastl::fixed_vector<Worker, 16, false> m_Workers = {};
    
    constexpr static usize kMaxJobSize = 64;
    JobFn m_pJobs[kMaxJobSize] = {};
    eastl::atomic<u64> m_JobsMask = 0;
    eastl::atomic<u64> m_StatusMask = 0;
};
}  // namespace lr::Job