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
    // For some reason, having Access::None for layout Undefined causes sync hazard
    // This might be exclusive to my GTX 1050 Ti --- sync2 error
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

    TaskBatchID latestBatch = 0;
    task.ForEachRes(
        [&](GenericResource &image)
        {
            auto &imageInfo = m_ImageInfos[image.m_ImageID];
            bool isLastRead = imageInfo.m_LastAccess.m_Access == MemoryAccess::Read;
            bool isCurrentRead = image.m_Access.m_Access == MemoryAccess::Read;
            bool isSameLayout = imageInfo.m_LastLayout == image.m_ImageLayout;

            if (!(isLastRead && isCurrentRead && isSameLayout))
                latestBatch = eastl::max(latestBatch, imageInfo.m_LastBatchIndex);
        },
        [&](GenericResource &buffer) {});

    if (latestBatch >= m_Batches.size())
        m_Batches.resize(latestBatch + 1);

    return latestBatch;
}

void TaskGraph::AddTask(Task &task, TaskID id)
{
    ZoneScoped;

    TaskBatchID batchID = ScheduleTask(task);
    auto &batch = m_Batches[batchID];
    task.ForEachRes(
        [&](GenericResource &image)
        {
            auto &imageInfo = m_ImageInfos[image.m_ImageID];
            bool isLastRead = imageInfo.m_LastAccess.m_Access == MemoryAccess::Read;
            bool isCurrentRead = image.m_Access.m_Access == MemoryAccess::Read;
            bool isSameLayout = imageInfo.m_LastLayout == image.m_ImageLayout;

            if (isLastRead && isCurrentRead && isSameLayout)  // Memory barrier
            {
                batch.m_ExecutionAccess = batch.m_ExecutionAccess | image.m_Access;
            }
            else  // Transition barrier
            {
                usize barrierID = m_Barriers.size();
                m_Barriers.push_back({
                    .m_ImageID = image.m_ImageID,
                    .m_SrcLayout = imageInfo.m_LastLayout,
                    .m_DstLayout = image.m_ImageLayout,
                    .m_SrcAccess = imageInfo.m_LastAccess,
                    .m_DstAccess = image.m_Access,
                });
                batch.m_WaitBarriers.push_back(barrierID);
            }

            // Update last image info
            imageInfo.m_LastLayout = image.m_ImageLayout;
            imageInfo.m_LastAccess = image.m_Access;
            imageInfo.m_LastBatchIndex = batchID;
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

    TaskAccess::Access lastExecutionAccess = {};
    for (auto &batch : m_Batches)
    {
        TaskCommandList taskList(pList);

        if (batch.m_ExecutionAccess != TaskAccess::None)
        {
            TaskBarrier executionBarrier = {
                .m_SrcAccess = lastExecutionAccess,
                .m_DstAccess = batch.m_ExecutionAccess,
            };

            InsertBarrier(taskList, executionBarrier);
        }

        for (auto barrierID : batch.m_WaitBarriers)
            InsertBarrier(taskList, m_Barriers[barrierID]);

        taskList.FlushBarriers();
        for (auto &pTask : m_Tasks)
        {
            TaskContext ctx(this, pList);
            pTask->Execute(ctx);
        }

        for (auto &barrierID : batch.m_EndBarriers)
            InsertBarrier(taskList, m_Barriers[barrierID]);

        taskList.FlushBarriers();
        lastExecutionAccess = batch.m_ExecutionAccess;
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
void TaskGraph::InsertBarrier(TaskCommandList &cmdList, const TaskBarrier &barrier)
{
    ZoneScoped;

    PipelineBarrier pipelineInfo = {
        .m_SrcStage = barrier.m_SrcAccess.m_Stage,
        .m_DstStage = barrier.m_DstAccess.m_Stage,
        .m_SrcAccess = barrier.m_SrcAccess.m_Access,
        .m_DstAccess = barrier.m_DstAccess.m_Access,
    };

    if (barrier.m_ImageID == ImageNull)
    {
        MemoryBarrier barrierInfo(pipelineInfo);
        cmdList.InsertMemoryBarrier(barrierInfo);
    }
    else
    {
        pipelineInfo.m_SrcLayout = barrier.m_SrcLayout;
        pipelineInfo.m_DstLayout = barrier.m_DstLayout;

        auto &imageInfo = m_ImageInfos[barrier.m_ImageID];
        ImageBarrier barrierInfo(imageInfo.m_pImage, {}, pipelineInfo);
        cmdList.InsertImageBarrier(barrierInfo);
    }
}

}  // namespace lr::Renderer