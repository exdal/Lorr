// Created on Tuesday May 16th 2023 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#pragma once

#include <eathread/eathread_thread.h>

#include "Memory/RingBuffer.hh"

namespace lr::Job
{
using JobFn = eastl::function<void()>;

struct Worker
{
    void Init(u32 id, u32 processor);
    void SetBusy();
    void SetIdle();

    u32 m_ID = 0;
    u32 m_Processor = 0;
    EA::Thread::Thread m_Thread;
};

struct JobManager
{
    static void Init(u32 threadCount);
    static void Schedule(JobFn fn);
    static void WaitForAll();
    static JobManager &Get();

    eastl::fixed_vector<Worker, 16, false> m_Workers = {};
    Memory::RingBufferAtomic<JobFn, 64> m_Jobs = {};
    eastl::atomic<u64> m_StatusMask = 0;
};
}  // namespace lr::Job