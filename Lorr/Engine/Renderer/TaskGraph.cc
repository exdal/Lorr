#include "TaskGraph.hh"

#include "TaskTypes.hh"

namespace lr::Graphics
{
void TaskGraph::init(TaskGraphDesc *desc)
{
    ZoneScoped;

    m_device = desc->m_device;

    m_frames.resize(desc->m_swap_chain->m_frame_count);
    for (auto &frame : m_frames)
    {
        m_device->create_timeline_semaphore(&frame.m_timeline_sema, 0);

        for (u32 i = 0; i < frame.m_allocators.max_size(); i++)
        {
            constexpr static auto k_allocator_flags = CommandAllocatorFlag::ResetCommandBuffer;
            CommandAllocator &allocator = frame.m_allocators[i];
            CommandList &list = frame.m_lists[i];

            m_device->create_command_allocator(&allocator, static_cast<CommandType>(i), k_allocator_flags);
            m_device->create_command_list(&list, &allocator);
        }
    }
    m_device->create_command_queue(&m_graphics_queue, CommandType::Graphics);
    m_pipeline_manager.init(m_device);
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
                batch.m_execution_access |= image.m_access;
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
                batch.m_execution_access |= buffer.m_access;
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

void TaskGraph::execute(SwapChain *swap_chain)
{
    ZoneScoped;

    u32 frame_id = swap_chain->m_current_frame_id;
    TaskFrame &frame = m_frames[frame_id];
    Semaphore &sync_sema = frame.m_timeline_sema;
    Semaphore &acquire_sema = swap_chain->m_acquire_semas[frame_id];
    Semaphore &present_sema = swap_chain->m_present_semas[frame_id];

    m_device->wait_for_semaphore(&sync_sema, sync_sema.m_value);
    for (auto &cmd_allocator : frame.m_allocators)
        m_device->reset_command_allocator(&cmd_allocator);

    for (auto &cmd_list : frame.m_lists)
        m_device->begin_command_list(&cmd_list);

    TaskAccess::Access last_execution_access = {};
    for (auto &batch : m_batches)
    {
        CommandList &cmd_list = frame.m_lists[0];  // uhh, split barriers when?
        CommandBatcher batcher = &cmd_list;

        if (batch.m_execution_access != TaskAccess::None)
        {
            TaskBarrier executionBarrier = {
                .m_src_access = last_execution_access,
                .m_dst_access = batch.m_execution_access,
            };

            insert_barrier(batcher, executionBarrier);
        }

        for (auto barrierID : batch.m_wait_barriers)
            insert_barrier(batcher, m_barriers[barrierID]);

        batcher.flush_barriers();
        for (auto &task : m_tasks)
        {
            TaskContext ctx(*task, *this, cmd_list);
            task->execute(ctx);
        }

        for (auto &barrierID : batch.m_end_barriers)
            insert_barrier(batcher, m_barriers[barrierID]);

        batcher.flush_barriers();
        last_execution_access = batch.m_execution_access;
    }

    for (auto &cmd_list : frame.m_lists)
        m_device->end_command_list(&cmd_list);

    eastl::array<CommandListSubmitDesc, 3> list_submits = {};
    for (u32 i = 0; i < frame.m_lists.max_size(); i++)
        list_submits[i] = &frame.m_lists[i];

    SemaphoreSubmitDesc wait_submits[] = { { &acquire_sema, PipelineStage::TopOfPipe } };
    SemaphoreSubmitDesc signal_submits[] = {
        { &present_sema, PipelineStage::AllCommands },
        { &sync_sema, ++sync_sema.m_value, PipelineStage::AllCommands },
    };

    SubmitDesc submit_desc = {
        .m_wait_semas = wait_submits,
        .m_lists = list_submits,
        .m_signal_semas = signal_submits,
    };
    m_device->submit(&m_graphics_queue, &submit_desc);
    m_device->present(swap_chain, &m_graphics_queue);
}

void TaskGraph::insert_present_barrier(CommandBatcher &batcher, ImageID back_buffer_id)
{
    ZoneScoped;

    if (m_batches.empty())
        return;

    auto &image_info = m_image_infos[back_buffer_id];
    insert_barrier(
        batcher,
        {
            .m_image_id = back_buffer_id,
            .m_src_layout = image_info.m_last_layout,
            .m_dst_layout = ImageLayout::Present,
            .m_src_access = image_info.m_last_access,
            .m_dst_access = TaskAccess::BottomOfPipe,
        });
}

void TaskGraph::insert_barrier(CommandBatcher &batcher, const TaskBarrier &barrier)
{
    ZoneScoped;

    PipelineBarrier pipelineInfo = {
        .m_src_stage = barrier.m_src_access.m_stage,
        .m_dst_stage = barrier.m_dst_access.m_stage,
        .m_src_access = barrier.m_src_access.m_access,
        .m_dst_access = barrier.m_dst_access.m_access,
    };

    if (barrier.m_image_id == LR_NULL_ID)
    {
        MemoryBarrier barrierInfo(pipelineInfo);
        batcher.insert_memory_barrier(barrierInfo);
    }
    else
    {
        pipelineInfo.m_src_layout = barrier.m_src_layout;
        pipelineInfo.m_dst_layout = barrier.m_dst_layout;

        auto &image_info = m_image_infos[barrier.m_image_id];
        ImageBarrier barrier_info(image_info.m_image, {}, pipelineInfo);
        batcher.insert_image_barrier(barrier_info);
    }
}
}  // namespace lr::Graphics
