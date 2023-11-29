#include "TaskGraph.hh"

#include "TaskTypes.hh"

namespace lr::Renderer
{
using namespace Graphics;

void TaskGraph::create(TaskGraphDesc *desc)
{
    ZoneScoped;

    m_device = desc->m_device;

    u32 graphicsIndex = desc->m_physical_device->get_queue_index(CommandType::Graphics);
    for (int i = 0; i < desc->m_frame_count; ++i)
    {
        m_semaphores.push_back(m_device->create_timeline_semaphore(0));
        // TODO: Split barriers
        CommandAllocator *pAllocator = m_device->create_command_allocator(graphicsIndex, CommandAllocatorFlag::ResetCommandBuffer);
        m_command_allocators.push_back(pAllocator);
        m_command_lists.push_back(m_device->create_command_list(pAllocator));
    }

    m_graphics_queue = m_device->create_command_queue(CommandType::Graphics, graphicsIndex);
    DeviceMemoryDesc imageMem = {
        .m_type = AllocatorType::TLSF,
        .m_flags = MemoryFlag::Device,
        .m_size = 0xffffff,
        .m_max_allocations = 4096,
    };
    m_image_memory = m_device->create_device_memory(&imageMem, desc->m_physical_device);

    m_task_allocator = { desc->m_initial_alloc };
}

ImageID TaskGraph::use_persistent_image(const PersistentImageInfo &persistentInfo)
{
    ZoneScoped;

    ImageID id = m_image_infos.size();
    // For some reason, having Access::None for layout Undefined causes sync hazard
    // This might be exclusive to my GTX 1050 Ti --- sync2 error
    m_image_infos.push_back({ .m_last_access = TaskAccess::TopOfPipe });
    set_image(id, persistentInfo.m_image, persistentInfo.m_view);

    return id;
}

void TaskGraph::set_image(ImageID image_id, Image *image, ImageView *view)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_image_infos[image_id];
    imageInfo.m_image = image;
    imageInfo.m_view = view;
}

BufferID TaskGraph::use_persistent_buffer(const PersistentBufferInfo &buffer_info)
{
    ZoneScoped;

    BufferID id = m_buffer_infos.size();
    m_buffer_infos.push_back({
        .m_buffer = buffer_info.m_buffer,
        .m_last_access = buffer_info.m_initial_access,
    });

    return id;
}

TaskBatchID TaskGraph::schedule_task(Task &task)
{
    ZoneScoped;

    TaskBatchID latest_batch = 0;
    task.for_each_res(
        [&](GenericResource &image)
        {
            auto &image_info = m_image_infos[image.m_image_id];
            bool is_last_read = image_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = image.m_access.m_access == MemoryAccess::Read;
            bool is_same_layout = image_info.m_last_layout == image.m_image_layout;

            if (!(is_last_read && is_current_read && is_same_layout))
                latest_batch = eastl::max(latest_batch, image_info.m_last_batch_index);
        },
        [&](GenericResource &buffer)
        {
            auto &buffer_info = m_buffer_infos[buffer.m_image_id];
            bool is_last_read = buffer_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = buffer.m_access.m_access == MemoryAccess::Read;

            if (!(is_last_read && is_current_read))
                latest_batch = eastl::max(latest_batch, buffer_info.m_last_batch_index);
        });

    if (latest_batch >= m_batches.size())
        m_batches.resize(latest_batch + 1);

    return latest_batch;
}

void TaskGraph::add_task(Task &task, TaskID id)
{
    ZoneScoped;

    TaskBatchID batch_id = schedule_task(task);
    auto &batch = m_batches[batch_id];
    task.for_each_res(
        [&](GenericResource &image)
        {
            auto &image_info = m_image_infos[image.m_image_id];
            bool is_last_read = image_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = image.m_access.m_access == MemoryAccess::Read;
            bool is_same_layout = image_info.m_last_layout == image.m_image_layout;

            if (is_last_read && is_current_read && is_same_layout)  // Memory barrier
            {
                batch.m_execution_access = batch.m_execution_access | image.m_access;
            }
            else  // Transition barrier
            {
                usize barrierID = m_barriers.size();
                m_barriers.push_back({
                    .m_image_id = image.m_image_id,
                    .m_src_layout = image_info.m_last_layout,
                    .m_dst_layout = image.m_image_layout,
                    .m_src_access = image_info.m_last_access,
                    .m_dst_access = image.m_access,
                });
                batch.m_wait_barriers.push_back(barrierID);
            }

            // Update last image info
            image_info.m_last_layout = image.m_image_layout;
            image_info.m_last_access = image.m_access;
            image_info.m_last_batch_index = batch_id;
        },
        [&](GenericResource &buffer)
        {
            auto &buffer_info = m_buffer_infos[buffer.m_buffer_id];
            bool is_last_read = buffer_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = buffer.m_access.m_access == MemoryAccess::Read;

            if (is_last_read && is_current_read)  // Memory barrier
            {
                batch.m_execution_access = batch.m_execution_access | buffer.m_access;
            }
            else  // Transition barrier
            {
                usize barrierID = m_barriers.size();
                m_barriers.push_back({
                    .m_src_access = buffer_info.m_last_access,
                    .m_dst_access = buffer.m_access,
                });
                batch.m_wait_barriers.push_back(barrierID);
            }

            // Update last buffer info
            buffer_info.m_last_access = buffer.m_access;
            buffer_info.m_last_batch_index = batch_id;
        });
}

void TaskGraph::present_task(ImageID back_buffer_id)
{
    ZoneScoped;

    if (m_batches.empty())
        return;

    auto &last_batch = m_batches.back();
    auto &image_info = m_image_infos[back_buffer_id];

    usize barrierID = m_barriers.size();
    m_barriers.push_back({
        .m_image_id = back_buffer_id,
        .m_src_layout = image_info.m_last_layout,
        .m_dst_layout = ImageLayout::Present,
        .m_src_access = image_info.m_last_access,
        .m_dst_access = TaskAccess::BottomOfPipe,
    });

    last_batch.m_end_barriers.push_back(barrierID);
}

void TaskGraph::execute(const TaskGraphExecuteDesc &desc)
{
    ZoneScoped;

    auto semaphore = m_semaphores[desc.m_FrameIndex];
    auto command_allocator = m_command_allocators[desc.m_FrameIndex];
    auto list = m_command_lists[desc.m_FrameIndex];

    m_device->wait_for_semaphore(semaphore, semaphore->m_value);
    m_device->reset_command_allocator(command_allocator);
    m_device->begin_command_list(list);

    TaskAccess::Access last_execution_access = {};
    for (auto &batch : m_batches)
    {
        TaskCommandList task_list = list;

        if (batch.m_execution_access != TaskAccess::None)
        {
            TaskBarrier executionBarrier = {
                .m_src_access = last_execution_access,
                .m_dst_access = batch.m_execution_access,
            };

            insert_barrier(task_list, executionBarrier);
        }

        for (auto barrierID : batch.m_wait_barriers)
            insert_barrier(task_list, m_barriers[barrierID]);

        task_list.flush_barriers();
        for (auto &pTask : m_tasks)
        {
            TaskContext ctx(*this, list);
            pTask->execute(ctx);
        }

        for (auto &barrierID : batch.m_end_barriers)
            insert_barrier(task_list, m_barriers[barrierID]);

        task_list.flush_barriers();
        last_execution_access = batch.m_execution_access;
    }

    m_device->end_command_list(list);

    /// END OF RENDERING --- SUBMIT ///

    SemaphoreSubmitDesc pWaitSubmits[] = {
        { desc.m_pAcquireSema, PipelineStage::TopOfPipe },
    };
    CommandListSubmitDesc pListSubmits[] = { list };
    SemaphoreSubmitDesc pSignalSubmits[] = {
        { desc.m_pPresentSema, PipelineStage::AllCommands },
        { semaphore, ++semaphore->m_value, PipelineStage::AllCommands },
    };

    SubmitDesc submitDesc = {
        .m_wait_semas = pWaitSubmits,
        .m_lists = pListSubmits,
        .m_signal_semas = pSignalSubmits,
    };

    m_device->submit(m_graphics_queue, &submitDesc);
}

void TaskGraph::insert_barrier(TaskCommandList &cmd_list, const TaskBarrier &barrier)
{
    ZoneScoped;

    PipelineBarrier pipelineInfo = {
        .m_src_stage = barrier.m_src_access.m_stage,
        .m_dst_stage = barrier.m_dst_access.m_stage,
        .m_src_access = barrier.m_src_access.m_access,
        .m_dst_access = barrier.m_dst_access.m_access,
    };

    if (barrier.m_image_id == ImageNull)
    {
        MemoryBarrier barrierInfo(pipelineInfo);
        cmd_list.insert_memory_barrier(barrierInfo);
    }
    else
    {
        pipelineInfo.m_src_layout = barrier.m_src_layout;
        pipelineInfo.m_dst_layout = barrier.m_dst_layout;

        auto &image_info = m_image_infos[barrier.m_image_id];
        ImageBarrier barrier_info(image_info.m_image, {}, pipelineInfo);
        cmd_list.insert_image_barrier(barrier_info);
    }
}
}  // namespace lr::Renderer
