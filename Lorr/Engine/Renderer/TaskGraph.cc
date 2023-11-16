#include "TaskGraph.hh"

#include "TaskTypes.hh"

namespace lr::Renderer
{
using namespace Graphics;

void TaskGraph::Init(TaskGraphDesc *pDesc)
{
    ZoneScoped;

    m_pDevice = pDesc->m_pDevice;

    u32 graphicsIndex = pDesc->m_pPhysDevice->GetQueueIndex(CommandType::Graphics);
    for (int i = 0; i < pDesc->m_FrameCount; ++i)
    {
        m_Semaphores.push_back(m_pDevice->CreateTimelineSemaphore(0));
        // TODO: Split barriers
        CommandAllocator *pAllocator = m_pDevice->CreateCommandAllocator(
            graphicsIndex, CommandAllocatorFlag::ResetCommandBuffer);
        m_CommandAllocators.push_back(pAllocator);
        m_CommandLists.push_back(m_pDevice->CreateCommandList(pAllocator));
    }

    m_pGraphicsQueue =
        m_pDevice->CreateCommandQueue(CommandType::Graphics, graphicsIndex);
    DeviceMemoryDesc imageMem = {
        .m_Type = AllocatorType::TLSF,
        .m_Flags = MemoryFlag::Device,
        .m_Size = 0xffffff,
    };
    m_pImageMemory = m_pDevice->CreateDeviceMemory(&imageMem, pDesc->m_pPhysDevice);

    m_TaskAllocator.Init({
        .m_DataSize = pDesc->m_InitialAlloc,
        .m_AllowMultipleBlocks = true,
    });
}

ImageID TaskGraph::UsePersistentImage(const PersistentImageInfo &persistentInfo)
{
    ZoneScoped;

    ImageID id = m_ImageInfos.size();
    m_ImageInfos.push_back({ .m_LastAccess = TaskAccess::TopOfPipe });
    SetImage(id, persistentInfo.m_pImage, persistentInfo.m_pImageView);

    return id;
}

void TaskGraph::SetImage(ImageID imageID, Image *pImage, ImageView *pView)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_ImageInfos[imageID];
    imageInfo.m_pImage = pImage;
    imageInfo.m_pImageView = pView;
}

TaskBatchID TaskGraph::ScheduleTask(Task &task)
{
    ZoneScoped;

    // TODO(Batching): The dream must come true...
    TaskBatchID batchID = m_Batches.size();
    m_Batches.push_back({});
    return batchID;
}

void TaskGraph::AddTask(Task &task, TaskID id)
{
    ZoneScoped;

    TaskBatchID batchID = ScheduleTask(task);
    auto &batch = m_Batches[batchID];
    task.ForEachRes(
        [&](GenericResource &image)
        {
            // TODO(Batching): This has to go, we can use normal memory barriers when
            // there are no image transition happening...
            auto &imageInfo = m_ImageInfos[image.m_ImageID];
            usize barrierID = m_Barriers.size();
            m_Barriers.push_back({
                .m_ImageID = image.m_ImageID,
                .m_SrcLayout = imageInfo.m_LastLayout,
                .m_DstLayout = image.m_ImageLayout,
                .m_SrcAccess = imageInfo.m_LastAccess,
                .m_DstAccess = image.m_Access,
            });
            batch.m_WaitBarriers.push_back(barrierID);

            // Update last image info
            imageInfo.m_LastLayout = image.m_ImageLayout;
            imageInfo.m_LastAccess = image.m_Access;
        },
        [&](GenericResource &buffer) {});
}
void TaskGraph::PresentTask(ImageID backBufferID)
{
    ZoneScoped;

    if (m_Batches.size() == 0)
        return;

    auto &lastBatch = m_Batches.back();
    auto &imageInfo = m_ImageInfos[backBufferID];

    usize barrierID = m_Barriers.size();
    m_Barriers.push_back({
        .m_ImageID = backBufferID,
        .m_SrcLayout = imageInfo.m_LastLayout,
        .m_DstLayout = ImageLayout::Present,
        .m_SrcAccess = imageInfo.m_LastAccess,
        .m_DstAccess = TaskAccess::BottomOfPipe,
    });

    lastBatch.m_EndBarriers.push_back(barrierID);
}

void TaskGraph::Execute(const TaskGraphExecuteDesc &desc)
{
    ZoneScoped;

    auto pSemaphore = m_Semaphores[desc.m_FrameIndex];
    auto pCmdAllocator = m_CommandAllocators[desc.m_FrameIndex];
    auto pList = m_CommandLists[desc.m_FrameIndex];

    m_pDevice->WaitForSemaphore(pSemaphore, pSemaphore->m_Value);
    m_pDevice->ResetCommandAllocator(pCmdAllocator);
    m_pDevice->BeginCommandList(pList);

    for (auto &batch : m_Batches)
    {
        for (auto barrierID : batch.m_WaitBarriers)
        {
            auto &barrier = m_Barriers[barrierID];
            if (barrier.m_ImageID == ImageNull)
                continue;  // TODO: Handle memory barriers

            auto &imageInfo = m_ImageInfos[barrier.m_ImageID];
            PipelineBarrier pipelineInfo = {
                .m_SrcLayout = barrier.m_SrcLayout,
                .m_DstLayout = barrier.m_DstLayout,
                .m_SrcStage = barrier.m_SrcAccess.m_Stage,
                .m_DstStage = barrier.m_DstAccess.m_Stage,
                .m_SrcAccess = barrier.m_SrcAccess.m_Access,
                .m_DstAccess = barrier.m_DstAccess.m_Access,
            };
            ImageBarrier imgDependency(imageInfo.m_pImage, {}, pipelineInfo);
            DependencyInfo dependencyInfo(imgDependency, {});
            pList->SetPipelineBarrier(&dependencyInfo);
        }

        for (auto &pTask : m_Tasks)
        {
            TaskContext ctx(this, pList);
            pTask->Execute(ctx);
        }

        for (auto &barrierID : batch.m_EndBarriers)
        {
            auto &barrier = m_Barriers[barrierID];
            if (barrier.m_ImageID == ImageNull)
                continue;  // TODO: Handle memory barriers

            auto &imageInfo = m_ImageInfos[barrier.m_ImageID];
            PipelineBarrier pipelineInfo = {
                .m_SrcLayout = barrier.m_SrcLayout,
                .m_DstLayout = barrier.m_DstLayout,
                .m_SrcStage = barrier.m_SrcAccess.m_Stage,
                .m_DstStage = barrier.m_DstAccess.m_Stage,
                .m_SrcAccess = barrier.m_SrcAccess.m_Access,
                .m_DstAccess = barrier.m_DstAccess.m_Access,
            };
            ImageBarrier imgDependency(imageInfo.m_pImage, {}, pipelineInfo);
            DependencyInfo dependencyInfo(imgDependency, {});
            pList->SetPipelineBarrier(&dependencyInfo);
        }
    }

    m_pDevice->EndCommandList(pList);

    /// END OF RENDERING --- SUBMIT ///

    SemaphoreSubmitDesc pWaitSubmits[] = {
        { desc.m_pAcquireSema, PipelineStage::TopOfPipe },
    };
    CommandListSubmitDesc pListSubmits[] = { pList };
    SemaphoreSubmitDesc pSignalSubmits[] = {
        { desc.m_pPresentSema, PipelineStage::AllCommands },
        { pSemaphore, ++pSemaphore->m_Value, PipelineStage::AllCommands },
    };

    SubmitDesc submitDesc = {
        .m_WaitSemas = pWaitSubmits,
        .m_Lists = pListSubmits,
        .m_SignalSemas = pSignalSubmits,
    };

    m_pDevice->Submit(m_pGraphicsQueue, &submitDesc);
}

}  // namespace lr::Renderer