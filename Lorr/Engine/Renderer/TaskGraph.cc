#include "TaskGraph.hh"

#include "TaskTypes.hh"

namespace lr::Graphics
{
void TaskGraph::init(TaskGraphDesc *desc)
{
    ZoneScoped;

    m_device = desc->m_device;

    m_frames.resize(desc->m_frame_count);
    for (auto &frame : m_frames)
    {
        m_device->create_timeline_semaphore(&frame.m_timeline_sema, 0);

        for (u32 i = 0; i < frame.m_allocators.max_size(); i++)
        {
            constexpr static auto k_allocator_flags = CommandAllocatorFlag::ResetCommandBuffer;
            CommandAllocator &allocator = frame.m_allocators[i];
            CommandList &list = frame.m_lists[i];

            m_device->create_command_allocator(&allocator, static_cast<CommandTypeMask>(1 << i), k_allocator_flags);
            m_device->create_command_list(&list, &allocator);
        }
    }

    m_pipeline_manager.init(m_device);
    m_task_allocator.init(0xffffa);
}

TaskGraph::~TaskGraph()
{
    for (auto &frame : m_frames)
    {
        m_device->delete_semaphore(&frame.m_timeline_sema);
        for (u32 i = 0; i < frame.m_allocators.size(); i++)
        {
            m_device->delete_command_list(&frame.m_lists[i], &frame.m_allocators[i]);
            m_device->delete_command_allocator(&frame.m_allocators[i]);
        }
    }

    for (auto &image : m_images.m_resources)
        m_device->delete_image(&image);
    for (auto &image_view : m_image_views.m_resources)
        m_device->delete_image_view(&image_view);

    for (auto &buffer : m_buffers.m_resources)
        m_device->delete_buffer(&buffer);
}

eastl::tuple<ImageID, Image *> TaskGraph::add_new_image()
{
    ZoneScoped;

    return m_images.add_resource();
}

eastl::tuple<ImageID, ImageView *> TaskGraph::add_new_image_view()
{
    ZoneScoped;

    return m_image_views.add_resource();
}

Image *TaskGraph::get_image(ImageID id)
{
    auto &image_info = m_image_infos[id];
    return &m_images.get_resource(image_info.m_image_id);
}

ImageView *TaskGraph::get_image_view(ImageID id)
{
    auto &image_info = m_image_infos[id];
    return &m_image_views.get_resource(image_info.m_image_view_id);
}

ImageID TaskGraph::use_persistent_image(const PersistentImageInfo &persistent_image_info)
{
    ZoneScoped;

    ImageID id = m_image_infos.size();
    m_image_infos.push_back({
        .m_image_id = persistent_image_info.m_image_id,
        .m_image_view_id = persistent_image_info.m_image_view_id,
        .m_last_access = TaskAccess::TopOfPipe,
    });
    set_image(id, persistent_image_info.m_image_id, persistent_image_info.m_image_view_id);

    return id;
}

void TaskGraph::set_image(ImageID image_info_id, ImageID image_id, ImageID image_view_id)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_image_infos[image_info_id];
    imageInfo.m_image_id = image_id;
    imageInfo.m_image_view_id = image_view_id;
}

BufferID TaskGraph::use_persistent_buffer(const PersistentBufferInfo &buffer_info)
{
    ZoneScoped;

    BufferID id = m_buffer_infos.size();
    m_buffer_infos.push_back({
        .m_buffer_id = buffer_info.m_buffer_id,
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
            auto &image_info = m_image_infos[image.m_task_image_id];
            bool is_last_read = image_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = image.m_access.m_access == MemoryAccess::Read;
            bool is_same_layout = image_info.m_last_layout == image.m_image_layout;

            if (!(is_last_read && is_current_read && is_same_layout))
                latest_batch = eastl::max(latest_batch, image_info.m_last_batch_index);
        },
        [&](GenericResource &buffer)
        {
            auto &buffer_info = m_buffer_infos[buffer.m_task_image_id];
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

    compile_task_pipeline(task);

    TaskBatchID batch_id = schedule_task(task);
    auto &batch = m_batches[batch_id];
    task.for_each_res(
        [&](GenericResource &image)
        {
            auto &image_info = m_image_infos[image.m_task_image_id];
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
                    .m_image_id = image.m_task_image_id,
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
            auto &buffer_info = m_buffer_infos[buffer.m_task_buffer_id];
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

void TaskGraph::compile_task_pipeline(Task &task)
{
    ZoneScoped;

    eastl::vector<Format> color_formats = {};
    Format depth_format = {};
    for (auto &resource : task.m_generic_resources)
    {
        if (resource.m_task_image_id == LR_NULL_ID)
            continue;

        ImageView *image_view = get_image_view(resource.m_task_image_id);
        if (resource.m_image_layout == ImageLayout::ColorAttachment)
            color_formats.push_back(image_view->m_format);
        else if (resource.m_image_layout == ImageLayout::DepthStencilAttachment)
            depth_format = image_view->m_format;
    }

    PipelineCompileInfo pipeline_compile_info = {};
    PipelineAttachmentInfo pipeline_attachment_info = { color_formats, depth_format };
    pipeline_compile_info.init(color_formats.size());

    if (task.compile_pipeline(pipeline_compile_info))
        task.m_pipeline_id = m_pipeline_manager.compile_pipeline(pipeline_compile_info, pipeline_attachment_info);
}

void TaskGraph::present(const TaskPresentDesc &present_desc)
{
    ZoneScoped;

    auto &image_info = m_image_infos[present_desc.m_swap_chain_image_id];

    auto &latest_batch = m_batches.back();
    usize barrier_id = m_barriers.size();
    m_barriers.push_back({
        .m_image_id = present_desc.m_swap_chain_image_id,
        .m_src_layout = image_info.m_last_layout,
        .m_dst_layout = ImageLayout::Present,
        .m_src_access = image_info.m_last_access,
        .m_dst_access = TaskAccess::BottomOfPipe,
    });
    latest_batch.m_end_barriers.push_back(barrier_id);
}

void TaskGraph::execute(const TaskExecuteDesc &execute_desc)
{
    ZoneScoped;

    u32 frame_id = execute_desc.m_swap_chain ? execute_desc.m_swap_chain->m_current_frame_id : 0;

    TaskFrame &frame = m_frames[frame_id];
    Semaphore &sync_sema = frame.m_timeline_sema;

    m_device->wait_for_semaphore(&sync_sema, sync_sema.m_value);
    for (auto &cmd_allocator : frame.m_allocators)
        m_device->reset_command_allocator(&cmd_allocator);

    CommandList &cmd_list = frame.m_lists[0];  // uhh, split barriers when?
    m_device->begin_command_list(&cmd_list);

    TaskAccess::Access last_execution_access = {};
    for (auto &batch : m_batches)
    {
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
    m_device->end_command_list(&cmd_list);

    SemaphoreSubmitDesc wait_submits[] = { { execute_desc.m_wait_semas[0], PipelineStage::TopOfPipe } };
    CommandListSubmitDesc list_submit_desc[] = { &cmd_list };
    SemaphoreSubmitDesc signal_submits[] = {
        { execute_desc.m_signal_semas[0], PipelineStage::AllCommands },
        { &sync_sema, ++sync_sema.m_value, PipelineStage::AllCommands },
    };

    SubmitDesc submit_desc = {
        .m_wait_semas = wait_submits,
        .m_lists = list_submit_desc,
        .m_signal_semas = signal_submits,
    };
    m_device->submit(m_device->get_queue(CommandType::Graphics), &submit_desc);
    m_device->present(execute_desc.m_swap_chain, m_device->get_queue(CommandType::Graphics));
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

        ImageBarrier barrier_info(get_image(barrier.m_image_id), {}, pipelineInfo);
        batcher.insert_image_barrier(barrier_info);
    }
}
}  // namespace lr::Graphics
