#pragma once

#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Memory/Allocator/LinearAllocator.hh"

#include <EASTL/functional.h>
#include <EASTL/vector.h>

namespace lr::Renderer
{
struct TaskContext;
using TaskExecuteCb = eastl::function<void(TaskContext &)>;
struct Task
{
    virtual void Execute(TaskContext &ctx) = 0;

    eastl::string_view m_Name;
    eastl::span<GenericResource> m_GenericResources;
};

struct TaskGroup
{
    GenericResource *GetLastAccess(GenericResource &resource);

    TaskGroup *m_pPrev = nullptr;
    TaskGroup *m_pNext = nullptr;
    
    Graphics::MemoryBarrier m_MemoryBarrier = {};
    // TODO: PLEASE MAKE THEM SPAN POINTING TO TLSF ALLOCATOR
    eastl::vector<Graphics::ImageBarrier> m_ImageBarriers;
    eastl::vector<Graphics::BufferBarrier> m_BufferBarriers;
    eastl::vector<Graphics::SemaphoreSubmitDesc> m_SplitBarriers;

    eastl::vector<Task *> m_Tasks;
};

struct TaskContext
{
    TaskContext(Graphics::CommandList *pList);
    Graphics::CommandList *GetCommandList();

private:
    Graphics::CommandList *m_pList = nullptr;
};

struct TaskGraphDesc
{
    usize m_InitialAlloc = 0xffff;
    usize m_InitialNodes = 16;
};

struct TaskGraph
{
    void Init(TaskGraphDesc *pDesc);
    void CompileGraph();
    void CompileTask(Task *pTask, TaskGroup *pGroup);

    TaskGroup *AllocateGroup(TaskGroup *pParent);
    TaskGroup *FindOptimalGroup(Task *pTask, TaskGroup *pDepGroup);
    template<typename _T>
    TaskGroup *AddTask(const _T::Uses &uses, TaskGroup *pDepGroup);

    TaskGroup *m_pHeadGroup = nullptr;
    Memory::LinearAllocator m_TaskAllocator = {};
    Memory::LinearAllocator m_GroupAllocator = {};
};

template<typename _T>
inline TaskGroup *TaskGraph::AddTask(const _T::Uses &uses, TaskGroup *pDepGroup)
{
    ZoneScoped;

    usize resourceCount = sizeof(uses) / sizeof(GenericResource);
    _T *pTaskData = m_TaskAllocator.New<_T>();
    Task *pTask = (Task *)pTaskData;

    pTaskData->m_Uses = uses;
    pTask->m_GenericResources = { (GenericResource *)&pTaskData->m_Uses, resourceCount };
    pTask->m_Name = _T::kName;

    return FindOptimalGroup(pTask, pDepGroup);
}

}  // namespace lr::Renderer