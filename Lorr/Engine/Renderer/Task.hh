#pragma once

#include "TaskResource.hh"

#include "Graphics/CommandList.hh"

namespace lr::Renderer
{
struct TaskGraph;
struct TaskContext
{
    TaskContext(TaskGraph *pGraph, Graphics::CommandList *pList)
        : m_pGraph(pGraph),
          m_pList(pList){};
    Graphics::CommandList *GetCommandList() { return m_pList; }
    Graphics::ImageView *View(const GenericResource &use);

private:
    TaskGraph *m_pGraph = nullptr;
    Graphics::CommandList *m_pList = nullptr;
};

using TaskID = u32;
struct Task
{
    virtual void Execute(TaskContext &ctx) = 0;

    template<typename _ImageFn, typename _BufferFn>
    void ForEachRes(const _ImageFn &fImg, const _BufferFn &fBuf)
    {
        for (GenericResource &resource : m_GenericResources)
        {
            switch (resource.m_Type)
            {
                case ResourceType::Buffer:
                    fBuf(resource);
                    break;
                case ResourceType::Image:
                    fImg(resource);
                    break;
            }
        }
    }

    eastl::string_view m_Name;
    eastl::span<GenericResource> m_GenericResources;
};

template<typename _T>
concept TaskConcept =
    requires { _T{}.m_Uses; } and requires(TaskContext tc) { _T{}.Execute(tc); };

template<TaskConcept _Task>
struct TaskWrapper : Task
{
    TaskWrapper(const _Task &task)
        : m_Task(task)
    {
        m_Name = _Task::kName;
        m_GenericResources = { (GenericResource *)&task.m_Uses, kResourceSize };
    }

    void Execute(TaskContext &ctx) override { m_Task.Execute(ctx); };

    _Task m_Task;
    constexpr static usize kResourceSize =
        sizeof(typename _Task::Uses) / sizeof(GenericResource);
};

struct TaskBarrier
{
    ImageID m_ImageID = ImageNull;
    Graphics::ImageLayout m_SrcLayout = {};
    Graphics::ImageLayout m_DstLayout = {};
    TaskAccess::Access m_SrcAccess = {};
    TaskAccess::Access m_DstAccess = {};
};

using TaskBatchID = u32;
struct TaskBatch
{
    TaskAccess::Access m_ExecutionAccess = TaskAccess::None;
    eastl::vector<usize> m_WaitBarriers = {};
    eastl::vector<TaskID> m_Tasks = {};

    // For resources are not connected to any other passes but still need a barrier
    // For example, SwapChain image needs to be in present layout at the end of execution
    eastl::vector<usize> m_EndBarriers = {};
};

constexpr static TaskBatchID kTaskBatchNull = ~0;

}  // namespace lr::Renderer