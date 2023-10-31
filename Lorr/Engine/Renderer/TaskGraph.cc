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
        Graphics::PipelineBarrier barrier = {
            .m_SrcLayout = Graphics::ImageLayout::Undefined,
            .m_SrcStage = Graphics::PipelineStage::TopOfPipe,
            .m_DstStage = resource.m_Stage,
            .m_SrcAccess = Graphics::MemoryAccess::None,
            .m_DstAccess = resource.m_Access,
        };

        if (pLastResource)
        {
            barrier.m_SrcLayout = pLastResource->m_ImageLayout;
            barrier.m_SrcAccess = pLastResource->m_Access;
            barrier.m_SrcStage = pLastResource->m_Stage;
        }

        switch (resource.m_Type)
        {
            case ResourceType::Buffer:
                pGroup->m_BufferBarriers.push_back({ resource.m_pBuffer, barrier });
                break;
            case ResourceType::Image:
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