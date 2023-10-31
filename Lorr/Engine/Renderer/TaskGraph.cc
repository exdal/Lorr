#include "TaskGraph.hh"

#include <EASTL/bonus/adaptors.h>

namespace lr::Renderer
{
GenericResource *TaskGroup::GetLastAccess(GenericResource &resource)
{
    ZoneScoped;

    TaskGroup *pCurGroup = m_pPrev;
    while (pCurGroup != nullptr)
    {
        for (auto &pTask : eastl::reverse(pCurGroup->m_Tasks))
            for (auto &passResource : eastl::reverse(pTask->m_GenericResources))
                if (passResource.m_pBuffer == resource.m_pBuffer)
                    return &passResource;

        pCurGroup = pCurGroup->m_pPrev;
    }

    return nullptr;
}

TaskContext::TaskContext(Graphics::CommandList *pList)
    : m_pList(pList)
{
}

Graphics::CommandList *TaskContext::GetCommandList()
{
    return m_pList;
}

void TaskGraph::Init(TaskGraphDesc *pDesc)
{
    ZoneScoped;

    m_TaskAllocator.Init({
        .m_DataSize = pDesc->m_InitialAlloc,
        .m_AllowMultipleBlocks = true,
    });

    m_GroupAllocator.Init({
        .m_DataSize = pDesc->m_InitialAlloc,
        .m_AllowMultipleBlocks = true,
    });

    m_pHeadGroup = AllocateGroup(nullptr);
}

void TaskGraph::CompileGraph()
{
    ZoneScoped;

    TaskGroup *pCurGroup = m_pHeadGroup;
    while (pCurGroup != nullptr)
    {
        for (Task *pTask : pCurGroup->m_Tasks)
            CompileTask(pTask, pCurGroup);

        pCurGroup = pCurGroup->m_pNext;
    }
}

void TaskGraph::CompileTask(Task *pTask, TaskGroup *pGroup)
{
    ZoneScoped;

    for (GenericResource &resource : pTask->m_GenericResources)
    {
        GenericResource *pLastResource = pGroup->GetLastAccess(resource);

        Graphics::MemoryAccess srcAccess =
            pLastResource ? pLastResource->m_Access : Graphics::MemoryAccess::None;
        Graphics::PipelineStage srcStage =
            pLastResource ? pLastResource->m_Stage : Graphics::PipelineStage::TopOfPiple;
        Graphics::PipelineBarrier barrier = {
            .m_SrcStage = srcStage,
            .m_DstStage = resource.m_Stage,
            .m_SrcAccess = srcAccess,
            .m_DstAccess = resource.m_Access,
        };

        switch (resource.m_Type)
        {
            case ResourceType::Buffer:
                pGroup->m_BufferBarriers.push_back({ resource.m_pBuffer, barrier });
                break;
            case ResourceType::Image:
                barrier.m_SrcLayout = pLastResource ? pLastResource->m_ImageLayout
                                                    : Graphics::ImageLayout::Undefined;
                barrier.m_DstLayout = resource.m_ImageLayout;
                pGroup->m_ImageBarriers.push_back({ resource.m_pImage, barrier });
                break;
        }
    }
}

TaskGroup *TaskGraph::AllocateGroup(TaskGroup *pParent)
{
    ZoneScoped;

    auto *pGroup = m_GroupAllocator.New<TaskGroup>();
    pGroup->m_pPrev = pParent;
    if (pParent)
        pParent->m_pNext = pGroup;

    return pGroup;
}

TaskGroup *TaskGraph::FindOptimalGroup(Task *pTask, TaskGroup *pDepGroup)
{
    ZoneScoped;

    TaskGroup *pGroup = nullptr;
    if (pDepGroup)
        pGroup = AllocateGroup(pDepGroup);
    else
        pGroup = m_pHeadGroup;

    pGroup->m_Tasks.push_back(pTask);

    return pGroup;
}

}  // namespace lr::Renderer