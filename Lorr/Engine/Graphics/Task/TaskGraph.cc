#include "TaskGraph.hh"

#include "Graphics/Device.hh"

#include "Memory/Stack.hh"

namespace lr::graphics {
void for_each_use(std::span<TaskUse> uses, const auto &buffer_fn, const auto &image_fn)
{
    for (TaskUse &use : uses) {
        switch (use.type) {
            case TaskUseType::Buffer:
                buffer_fn(use.task_buffer_id, use.access);
                break;
            case TaskUseType::Image:
                image_fn(use.task_image_id, use.access, use.image_layout);
                break;
        }
    }
}

bool TaskGraph::init(const TaskGraphInfo &info)
{
    ZoneScoped;

    m_device = info.device;

    m_device->create_command_allocators(m_command_allocators[0], { .type = CommandType::Graphics });
    m_device->create_command_allocators(m_command_allocators[1], { .type = CommandType::Compute });
    m_device->create_command_allocators(m_command_allocators[2], { .type = CommandType::Transfer });

    m_submits.push_back({});

    return true;
}

TaskImageID TaskGraph::add_image(const TaskPersistentImageInfo &info)
{
    ZoneScoped;

    TaskImageID task_image_id = static_cast<TaskImageID>(m_images.size());
    m_images.push_back({ .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access });

    return task_image_id;
}

void TaskGraph::set_image(TaskImageID task_image_id, const TaskPersistentImageInfo &info)
{
    ZoneScoped;

    usize index = static_cast<usize>(task_image_id);
    m_images[index] = { .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access };
}

u32 TaskGraph::schedule_task(Task *task, TaskSubmit &submit)
{
    ZoneScoped;

    u32 first_batch_index = 0;
    for_each_use(
        task->m_task_uses,
        [&](TaskBufferID id, const PipelineAccessImpl &access) { TaskBuffer &task_buffer = m_buffers[static_cast<usize>(id)]; },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = m_images[static_cast<usize>(id)];
            u32 cur_batch_index = task_image.last_batch_index;

            bool is_same_layout = layout == task_image.last_layout;
            bool is_last_none = task_image.last_access == PipelineAccess::None;
            bool is_read_on_read = access.access == MemoryAccess::Read && task_image.last_access.access == MemoryAccess::Read;

            if ((!is_same_layout && !is_read_on_read) && !is_last_none) {
                cur_batch_index++;
            }

            first_batch_index = std::max(first_batch_index, cur_batch_index);
        });

    if (first_batch_index >= submit.batches.size()) {
        submit.batches.resize(first_batch_index + 1);
    }

    return first_batch_index;
}

TaskID TaskGraph::add_task(std::unique_ptr<Task> &&task)
{
    ZoneScoped;

    TaskID task_id = static_cast<TaskID>(m_tasks.size());
    u32 submit_index = m_submits.size() - 1;
    TaskSubmit &current_submit = m_submits[submit_index];
    u32 batch_index = schedule_task(&*task, current_submit);
    TaskBatch &batch = current_submit.batches[batch_index];

    for_each_use(
        task->m_task_uses,
        [&](TaskBufferID id, const PipelineAccessImpl &access) { TaskBuffer &task_buffer = m_buffers[static_cast<usize>(id)]; },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = m_images[static_cast<usize>(id)];
            bool is_same_layout = layout == task_image.last_layout;

            if (!is_same_layout) {
                u32 barrier_id = m_barriers.size();
                m_barriers.push_back(TaskBarrier{
                    .src_layout = task_image.last_layout,
                    .dst_layout = layout,
                    .src_access = task_image.last_access,
                    .dst_access = access,
                    .image_id = id,
                });

                batch.barrier_indices.push_back(barrier_id);
            }

            task_image.last_layout = layout;
            task_image.last_access = access;
            task_image.last_batch_index = batch_index;
            task_image.last_submit_index = submit_index;
        });

    batch.tasks.push_back(task_id);
    m_tasks.push_back(std::move(task));

    return task_id;
}

void TaskGraph::present(TaskImageID task_image_id)
{
    ZoneScoped;

    TaskImage &task_image = m_images[static_cast<usize>(task_image_id)];
    TaskSubmit &task_submit = m_submits[task_image.last_submit_index];

    u32 barrier_id = m_barriers.size();
    m_barriers.push_back({
        .src_layout = task_image.last_layout,
        .dst_layout = ImageLayout::Present,
        .src_access = task_image.last_access,
        .dst_access = PipelineAccess::BottomOfPipe,
        .image_id = task_image_id,
    });
    task_submit.additional_signal_barrier_indices.push_back(barrier_id);
}

void TaskGraph::execute(const TaskExecuteInfo &info)
{
    ZoneScoped;
    memory::ScopedStack stack;

    for (u32 submit_index = 0; submit_index < m_submits.size(); submit_index++) {
        TaskSubmit &task_submit = m_submits[submit_index];
        CommandQueue &cmd_queue = m_device->get_queue(task_submit.type);
        Semaphore &queue_sema = cmd_queue.semaphore();
        CommandAllocator &cmd_allocator = m_command_allocators[static_cast<usize>(task_submit.type)][info.image_index];

        bool has_additional_barriers = !task_submit.additional_signal_barrier_indices.empty();
        auto cmd_list_submit_infos = stack.alloc<CommandListSubmitInfo>(task_submit.batches.size() + !!has_additional_barriers);

        usize batch_index = 0;
        for (TaskBatch &task_batch : task_submit.batches) {
            Unique<CommandList> cmd_list(m_device);
            CommandBatcher cmd_batcher(cmd_list);

            m_device->create_command_lists({ &*cmd_list, 1 }, cmd_allocator);
            m_device->begin_command_list(*cmd_list);

            for (u32 barrier_index : task_batch.barrier_indices) {
                TaskBarrier &barrier = m_barriers[barrier_index];
                TaskImage &barrier_task_image = m_images[static_cast<usize>(barrier.image_id)];
                cmd_batcher.insert_image_barrier({
                    .src_access = barrier.src_access,
                    .dst_access = barrier.dst_access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                });
            }
            cmd_batcher.flush_barriers();

            for (TaskID task_id : task_batch.tasks) {
                Task *task = &*m_tasks[static_cast<usize>(task_id)];
                TaskContext tc(m_images, *cmd_list);
                task->execute(tc);
            }

            m_device->end_command_list(*cmd_list);
            cmd_list_submit_infos[batch_index] = { *cmd_list };

            batch_index++;
        }

        if (!task_submit.additional_signal_barrier_indices.empty()) {
            Unique<CommandList> cmd_list(m_device);
            CommandBatcher cmd_batcher(cmd_list);

            m_device->create_command_lists({ &*cmd_list, 1 }, cmd_allocator);
            m_device->begin_command_list(*cmd_list);

            for (u32 barrier_index : task_submit.additional_signal_barrier_indices) {
                TaskBarrier &barrier = m_barriers[barrier_index];
                TaskImage &barrier_task_image = m_images[static_cast<usize>(barrier.image_id)];
                cmd_batcher.insert_image_barrier({
                    .src_access = barrier.src_access,
                    .dst_access = barrier.dst_access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                });
            }
            cmd_batcher.flush_barriers();

            m_device->end_command_list(*cmd_list);
            cmd_list_submit_infos.back() = { *cmd_list };
        }

        SemaphoreSubmitInfo queue_wait_semas[] = { { queue_sema, queue_sema.counter(), PipelineStage::AllCommands } };
        SemaphoreSubmitInfo queue_signal_semas[] = { { queue_sema, queue_sema.advance(), PipelineStage::AllCommands } };

        std::span<SemaphoreSubmitInfo> all_wait_semas = queue_wait_semas;
        std::span<SemaphoreSubmitInfo> all_signal_semas = queue_signal_semas;
        if (submit_index == 0) {
            all_wait_semas = stack.alloc<SemaphoreSubmitInfo>(info.wait_semas.size() + count_of(queue_wait_semas));
            usize i = 0;

            for (SemaphoreSubmitInfo &sema : info.wait_semas) {
                all_wait_semas[i++] = sema;
            }

            for (SemaphoreSubmitInfo &sema : queue_wait_semas) {
                all_wait_semas[i++] = sema;
            }
        }

        if (submit_index == m_submits.size() - 1) {
            all_signal_semas = stack.alloc<SemaphoreSubmitInfo>(info.signal_semas.size() + count_of(queue_signal_semas));
            usize i = 0;

            for (SemaphoreSubmitInfo &sema : info.signal_semas) {
                all_signal_semas[i++] = sema;
            }

            for (SemaphoreSubmitInfo &sema : queue_signal_semas) {
                all_signal_semas[i++] = sema;
            }
        }

        QueueSubmitInfo submit_info = {
            .wait_sema_count = static_cast<u32>(all_wait_semas.size()),
            .wait_sema_infos = all_wait_semas.data(),
            .command_list_count = static_cast<u32>(cmd_list_submit_infos.size()),
            .command_list_infos = cmd_list_submit_infos.data(),
            .signal_sema_count = static_cast<u32>(all_signal_semas.size()),
            .signal_sema_infos = all_signal_semas.data(),
        };
        cmd_queue.submit(submit_info);
    }
}

}  // namespace lr::graphics
