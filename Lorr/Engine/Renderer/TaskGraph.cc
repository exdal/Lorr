#include "TaskGraph.hh"

#include "TaskTypes.hh"

namespace lr::Graphics
{
void TaskGraph::init(TaskGraphDesc *desc)
{
    ZoneScoped;

    m_device = desc->m_device;
    m_swap_chain = desc->m_swap_chain;

    m_pipeline_manager.init(m_device);
    DescriptorManagerDesc descriptor_manager_desc = {
        .m_device = m_device,
        .m_persistent_descriptors = k_persistent_sets,
        .m_frame_count = m_swap_chain->m_frame_count,
        .m_dynamic_descriptors = k_dynamic_sets,
    };
    m_descriptor_manager.init(&descriptor_manager_desc);
    m_memory_allocator.init(m_device);
    m_task_allocator.init(0xffffa);

    m_frames.resize(m_swap_chain->m_frame_count);
    for (auto &frame : m_frames)
    {
        m_device->create_timeline_semaphore(&frame.m_timeline_sema, 0);
        frame.m_frametime_memory = m_memory_allocator.create_linear_memory_pool(MemoryPoolLocation::CPU, Memory::MiBToBytes(32));

        for (u32 i = 0; i < frame.m_allocators.max_size(); i++)
        {
            constexpr static auto k_allocator_flags = CommandAllocatorFlag::ResetCommandBuffer;
            CommandAllocator &allocator = frame.m_allocators[i];

            m_device->create_command_allocator(&allocator, static_cast<CommandType>(i), k_allocator_flags);
        }
    }
}

TaskGraph::~TaskGraph()
{
    for (auto &frame : m_frames)
    {
        m_device->delete_semaphore(&frame.m_timeline_sema);
        for (u32 i = 0; i < frame.m_allocators.size(); i++)
        {
            m_device->delete_command_allocator(&frame.m_allocators[i]);
        }
    }
}

TaskImageID TaskGraph::use_persistent_image(const PersistentImageInfo &persistent_image_info)
{
    ZoneScoped;

    auto id = set_handle_val<TaskImageID>(m_image_infos.size());
    m_image_infos.push_back({
        .m_image_id = persistent_image_info.m_image_id,
        .m_image_view_id = persistent_image_info.m_image_view_id,
        .m_last_access = TaskAccess::TopOfPipe,
    });
    set_image(id, persistent_image_info.m_image_id, persistent_image_info.m_image_view_id);

    return id;
}

void TaskGraph::set_image(TaskImageID task_image_id, ImageID image_id, ImageViewID image_view_id)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_image_infos[get_handle_val(task_image_id)];
    imageInfo.m_image_id = image_id;
    imageInfo.m_image_view_id = image_view_id;
}

TaskBufferID TaskGraph::use_persistent_buffer(const PersistentBufferInfo &buffer_info)
{
    ZoneScoped;

    auto id = set_handle_val<TaskBufferID>(m_buffer_infos.size());
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
            auto &image_info = m_image_infos[get_handle_val(image.m_task_image_id)];
            bool is_last_read = image_info.m_last_access.m_access == MemoryAccess::Read;
            bool is_current_read = image.m_access.m_access == MemoryAccess::Read;
            bool is_same_layout = image_info.m_last_layout == image.m_image_layout;

            if (!(is_last_read && is_current_read && is_same_layout))
                latest_batch = eastl::max(latest_batch, image_info.m_last_batch_index);
        },
        [&](GenericResource &buffer)
        {
            auto &buffer_info = m_buffer_infos[get_handle_val(buffer.m_task_buffer_id)];
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
            auto &image_info = m_image_infos[get_handle_val(image.m_task_image_id)];
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
                    .m_task_image_id = image.m_task_image_id,
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
            auto &buffer_info = m_buffer_infos[get_handle_val(buffer.m_task_buffer_id)];
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
        auto &image_info = m_image_infos[get_handle_val(resource.m_task_image_id)];
        ImageView *image_view = m_device->get_image_view(image_info.m_image_view_id);
        if (resource.m_image_layout == ImageLayout::ColorAttachment)
            color_formats.push_back(image_view->m_format);
        else if (resource.m_image_layout == ImageLayout::DepthStencilAttachment)
            depth_format = image_view->m_format;
    }

    PipelineCompileInfo pipeline_compile_info = {};
    PipelineAttachmentInfo pipeline_attachment_info = { color_formats, depth_format };
    pipeline_compile_info.init(color_formats.size());

    if (task.compile_pipeline(pipeline_compile_info))
        task.m_pipeline_info_id =
            m_pipeline_manager.compile_pipeline(pipeline_compile_info, pipeline_attachment_info, m_descriptor_manager.get_layout());
}

void TaskGraph::submit()
{
    ZoneScoped;

    usize submit_scope_id = m_submit_scopes.size();
    m_submit_scopes.push_back({});
    m_command_lists.resize(m_command_lists.size() + m_frames.size());

    for (u32 i = 0; i < m_frames.size(); i++)
    {
        CommandList &command_list = m_command_lists[submit_scope_id + i];
        m_device->create_command_list(&command_list, &m_frames[i].m_allocators[0]);  // TODO: Multi stage submit scopes
    }
}

void TaskGraph::present(const TaskPresentDesc &present_desc)
{
    ZoneScoped;

    TaskImageInfo &image_info = get_image_info(present_desc.m_swap_chain_image_id);

    TaskSubmitScope &submit_scope = m_submit_scopes[image_info.m_last_batch_index];
    usize barrier_id = m_barriers.size();
    m_barriers.push_back({
        .m_task_image_id = present_desc.m_swap_chain_image_id,
        .m_src_layout = image_info.m_last_layout,
        .m_dst_layout = ImageLayout::Present,
        .m_src_access = image_info.m_last_access,
        .m_dst_access = TaskAccess::BottomOfPipe,
    });
    submit_scope.m_end_barriers.push_back(barrier_id);
}

void TaskGraph::execute(const TaskExecuteDesc &execute_desc)
{
    ZoneScoped;

    u32 frame_id = m_swap_chain ? m_swap_chain->m_current_frame_id : 0;

    TaskFrame &frame = m_frames[frame_id];
    Semaphore &sync_sema = frame.m_timeline_sema;
    TaskImageID swap_chain_image = execute_desc.m_swap_chain_image_id;
    CommandAllocator &cmd_allocator = frame.m_allocators[0];

    m_device->wait_for_semaphore(&sync_sema, sync_sema.m_value);
    m_device->reset_command_allocator(&cmd_allocator);

    while (!frame.m_zombie_buffers.empty())
    {
        m_device->delete_buffer(frame.m_zombie_buffers.front());
        frame.m_zombie_buffers.pop();
    }

    usize curr_scope_idx = 0;
    for (TaskSubmitScope &submit_scope : m_submit_scopes)
    {
        CommandList &scope_cmd_list = m_command_lists[curr_scope_idx + frame_id];
        m_device->begin_command_list(&scope_cmd_list);
        m_descriptor_manager.bind_all(&scope_cmd_list, CommandType::Graphics, frame_id);

        CommandBatcher batcher = &scope_cmd_list;
        for (TaskBatch &batch : m_batches)
        {
            for (auto barrierID : batch.m_wait_barriers)
                insert_barrier(batcher, m_barriers[barrierID]);

            batcher.flush_barriers();
            for (auto &task : m_tasks)
            {
                TaskContext ctx(*task, *this, scope_cmd_list);
                task->execute(ctx);
            }
        }

        for (u32 barrier_id : submit_scope.m_end_barriers)
            insert_barrier(batcher, m_barriers[barrier_id]);

        batcher.flush_barriers();
        m_device->end_command_list(&scope_cmd_list);

        CommandListSubmitDesc list_submit_desc[] = { &scope_cmd_list };
        eastl::vector<SemaphoreSubmitDesc> wait_semas = {};
        eastl::vector<SemaphoreSubmitDesc> signal_semas = {};
        if (m_swap_chain && swap_chain_image != TaskImageID::Invalid)
        {
            TaskImageInfo &image_info = get_image_info(swap_chain_image);
            if (image_info.m_first_submit_scope_index == curr_scope_idx)
            {
                Semaphore &acquire_sema = m_swap_chain->m_acquire_semas[frame_id];
                wait_semas.push_back({ &acquire_sema, PipelineStage::TopOfPipe });
            }
            if (image_info.m_last_submit_scope_index == curr_scope_idx)
            {
                Semaphore &present_sema = m_swap_chain->m_present_semas[frame_id];
                signal_semas.push_back({ &present_sema, PipelineStage::AllCommands });
            }
        }

        signal_semas.push_back({ &sync_sema, ++sync_sema.m_value, PipelineStage::AllCommands });

        SubmitDesc submit_desc = {
            .m_wait_semas = wait_semas,
            .m_lists = list_submit_desc,
            .m_signal_semas = signal_semas,
        };
        m_device->submit(m_device->get_queue(CommandType::Graphics), &submit_desc);

        curr_scope_idx++;
    }

    m_device->present(m_swap_chain, m_device->get_queue(CommandType::Graphics));
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

    if (barrier.m_task_image_id == TaskImageID::Invalid)
    {
        MemoryBarrier barrierInfo(pipelineInfo);
        batcher.insert_memory_barrier(barrierInfo);
    }
    else
    {
        pipelineInfo.m_src_layout = barrier.m_src_layout;
        pipelineInfo.m_dst_layout = barrier.m_dst_layout;

        TaskImageInfo &image_info = get_image_info(barrier.m_task_image_id);
        ImageBarrier barrier_info(m_device->get_image(image_info.m_image_id), {}, pipelineInfo);
        batcher.insert_image_barrier(barrier_info);
    }
}
}  // namespace lr::Graphics
