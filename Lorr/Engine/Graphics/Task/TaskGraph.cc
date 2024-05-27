#include "TaskGraph.hh"

#include "Engine/Graphics/Device.hh"

#include "Engine/Memory/Stack.hh"

#include <sstream>

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
    m_device->create_timestamp_query_pools(m_task_query_pools, { .query_count = 512, .debug_name = "TG Task Query Pool" });
    new_submit(CommandType::Graphics);

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
        [&](TaskBufferID id, const PipelineAccessImpl &access) {
            TaskBuffer &task_buffer = m_buffers[static_cast<usize>(id)];
            u32 cur_batch_index = task_buffer.last_batch_index;

            bool is_read_on_read = access.access == MemoryAccess::Read && task_buffer.last_access.access == MemoryAccess::Read;
            if (!is_read_on_read) {
                cur_batch_index++;
            }

            first_batch_index = std::max(first_batch_index, cur_batch_index);
        },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = m_images[static_cast<usize>(id)];
            u32 cur_batch_index = task_image.last_batch_index;

            bool is_same_layout = layout == task_image.last_layout;
            bool is_last_none = task_image.last_access == PipelineAccess::None;
            bool is_read_on_read = access.access == MemoryAccess::Read && task_image.last_access.access == MemoryAccess::Read;

            if ((!is_same_layout || !is_read_on_read) && !is_last_none) {
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
        [&](TaskBufferID id, const PipelineAccessImpl &access) {
            TaskBuffer &task_buffer = m_buffers[static_cast<usize>(id)];
            bool is_read_on_read = access.access == MemoryAccess::Read && task_buffer.last_access.access == MemoryAccess::Read;
            if (!is_read_on_read) {
                batch.execution_access |= access;
            }

            task_buffer.last_access = access;
            task_buffer.last_batch_index = batch_index;
            task_buffer.last_submit_index = submit_index;
        },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = m_images[static_cast<usize>(id)];
            bool is_same_layout = layout == task_image.last_layout;
            bool is_read_on_read = access.access == MemoryAccess::Read && task_image.last_access.access == MemoryAccess::Read;

            if (!is_same_layout || !is_read_on_read) {
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

std::string TaskGraph::generate_graphviz()
{
    ZoneScoped;

    u32 batch_id = 0;

    std::stringstream ss;
    ss << "digraph task_graph {\n";
    ss << "graph [rankdir = \"LR\" compound = true];\n";
    ss << "node [fontsize = \"16\" shape = \"ellipse\"]\n";
    ss << "edge [];\n";
    for (u32 task_id = 0; task_id < m_tasks.size(); task_id++) {
        std::string_view name = m_tasks[task_id]->m_name;
        auto uses = m_tasks[task_id]->m_task_uses;

        ss << fmt::format(R"(task{} [)", task_id) << "\n";
        ss << fmt::format(R"(label = "<name> {}({}))", name, task_id);
        for (auto &use : uses) {
            ss << fmt::format(R"(|<i{}> image {})", use.index, use.index);
        }
        ss << "\"\n";
        ss << R"(shape = "record")" << "\n";
        ss << "];\n";
    }

    // mfw dot doesnt have proper subgraph connectors
    for (TaskSubmit &submit : m_submits) {
        for (u32 i = 0; i < submit.batches.size(); i++) {
            TaskBatch &batch = submit.batches[i];
            TaskID mid_task_id = batch.tasks[batch.tasks.size() / 2];

            ss << fmt::format(R"(subgraph cluster_{})", batch_id) << " {\n";
            ss << fmt::format(R"(label = "Batch {}\n{} barriers";)", batch_id, batch.barrier_indices.size()) << "\n";
            if (i != 0) {
                TaskBatch &last_batch = submit.batches[i - 1];
                TaskID last_mid_task_id = last_batch.tasks[last_batch.tasks.size() / 2];
                u32 lmtidx = static_cast<u32>(last_mid_task_id);
                u32 mtidx = static_cast<u32>(mid_task_id);
                ss << fmt::format("task{} -> task{} [ltail = cluster_{} lhead = cluster_{} label=\"todo barrier info\"];\n", lmtidx, mtidx, i - 1, i);
            }
            for (TaskID task_id : batch.tasks) {
                if (task_id == mid_task_id && i != 0) {
                    continue;
                }

                u32 task_index = static_cast<u32>(task_id);
                ss << fmt::format("task{} ", task_index);
            }

            ss << "\n}\n";
            batch_id++;
        }
    }

    ss << "}\n";

    return ss.str();
}

void TaskGraph::draw_profiler_ui()
{
    ZoneScoped;

    auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(ImVec2(750, 450), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Legit Profiler", nullptr);
    ImGui::Text("Frametime: %.4fms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    m_task_gpu_profiler_graph.LoadFrameData(m_task_gpu_profiler_tasks.data(), m_task_gpu_profiler_tasks.size());
    m_task_gpu_profiler_graph.RenderTimings(500, 20, 200, 0);
    ImGui::SliderFloat("Graph detail", &m_task_gpu_profiler_graph.maxFrameTime, 1.0f, 10000.f);

    ImGui::Separator();

    for (u32 submit_index = 0; submit_index < m_submits.size(); submit_index++) {
        TaskSubmit &submit = m_submits[submit_index];

        if (ImGui::TreeNode(&submit, "Submit %u", submit_index)) {
            for (u32 batch_index = 0; batch_index < submit.batches.size(); batch_index++) {
                TaskBatch &batch = submit.batches[batch_index];

                if (ImGui::TreeNode(&batch, "Batch %u - %lfms", batch_index, batch.execution_time())) {
                    ImGui::Text("%lu total barrier(s).", batch.barrier_indices.size());
                    ImGui::Text("%lu total task(s).", batch.tasks.size());

                    for (TaskID task_id : batch.tasks) {
                        Task *task = &*m_tasks[static_cast<usize>(task_id)];
                        ImGui::BulletText("%s - %lfms", task->m_name.data(), task->execution_time());
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void TaskGraph::execute(const TaskExecuteInfo &info)
{
    ZoneScoped;
    memory::ScopedStack stack;

    m_task_gpu_profiler_tasks.clear();

    TimestampQueryPool &task_query_pool = m_task_query_pools[info.image_index];
    std::span<u64> task_query_results = stack.alloc<u64>(m_tasks.size() * 4);
    m_device->get_timestamp_query_pool_results(task_query_pool, 0, m_tasks.size() * 2, task_query_results);

    f64 task_query_offset_ts = 0.0;
    for (u32 i = 0; i < task_query_results.size(); i += 4) {
        if (task_query_results[i + 1] == 0 && task_query_results[i + 3] == 0) {
            continue;
        }

        u32 task_index = i / 4;
        auto &task = m_tasks[task_index];
        task->m_start_ts = static_cast<f64>(task_query_results[i + 0]);
        task->m_end_ts = static_cast<f64>(task_query_results[i + 2]);

        f64 delta = ((task->m_end_ts / 1000000.0f) - (task->m_start_ts / 1000000.0f)) / 1000.0f;
        m_task_gpu_profiler_tasks.push_back({
            .startTime = task_query_offset_ts,
            .endTime = task_query_offset_ts + delta,
            .name = task->m_name.data(),
            .color = legit::Colors::colors[task_index % count_of(legit::Colors::colors)],
        });

        task_query_offset_ts += delta;
    }

    for (u32 submit_index = 0; submit_index < m_submits.size(); submit_index++) {
        TaskSubmit &task_submit = m_submits[submit_index];
        CommandQueue &cmd_queue = m_device->get_queue(task_submit.type);
        Semaphore &queue_sema = cmd_queue.semaphore();
        CommandAllocator &cmd_allocator = task_submit.frame_cmd_allocator(info.image_index);
        TimestampQueryPool &batch_query_pool = task_submit.frame_query_pool(info.image_index);

        std::span<u64> batch_query_results = stack.alloc<u64>(task_submit.batches.size() * 4);
        m_device->get_timestamp_query_pool_results(batch_query_pool, 0, task_submit.batches.size() * 2, batch_query_results);

        for (u32 i = 0; i < batch_query_results.size(); i += 4) {
            if (batch_query_results[i + 1] == 0 && batch_query_results[i + 3] == 0) {
                continue;
            }

            u32 batch_index = i / 4;
            TaskBatch &batch = task_submit.batches[batch_index];
            batch.start_ts = static_cast<f64>(batch_query_results[i + 0]);
            batch.end_ts = static_cast<f64>(batch_query_results[i + 2]);
        }

        bool has_additional_barriers = !task_submit.additional_signal_barrier_indices.empty();
        auto cmd_list_submit_infos = stack.alloc<CommandListSubmitInfo>(task_submit.batches.size() + !!has_additional_barriers);

        PipelineAccessImpl last_batch_access = PipelineAccess::TopOfPipe;
        usize batch_index = 0;
        for (TaskBatch &task_batch : task_submit.batches) {
            Unique<CommandList> cmd_list(m_device);
            CommandBatcher cmd_batcher(cmd_list);

            m_device->create_command_lists({ &*cmd_list, 1 }, cmd_allocator);
            m_device->begin_command_list(*cmd_list);
            cmd_list->reset_query_pool(batch_query_pool, batch_index * 2, 2);
            cmd_list->write_timestamp(batch_query_pool, PipelineStage::TopOfPipe, batch_index * 2);

            cmd_batcher.insert_memory_barrier({
                .src_access = last_batch_access,
                .dst_access = task_batch.execution_access,
            });

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
                u32 task_index = static_cast<u32>(task_id);
                Task *task = &*m_tasks[task_index];
                PipelineLayout &pipeline_layout = m_device->m_resources.pipeline_layouts[task->m_pipeline_layout_index];
                cmd_list->set_descriptor_sets(pipeline_layout, PipelineBindPoint::Compute, 0, { &m_device->m_descriptor_set, 1 });
                cmd_list->reset_query_pool(task_query_pool, task_index * 2, 2);
                cmd_list->write_timestamp(task_query_pool, PipelineStage::TopOfPipe, task_index * 2);

                TaskContext tc(m_images, *cmd_list, m_device);
                task->execute(tc);

                cmd_list->write_timestamp(task_query_pool, PipelineStage::BottomOfPipe, task_index * 2 + 1);
            }

            cmd_list->write_timestamp(batch_query_pool, PipelineStage::BottomOfPipe, batch_index * 2 + 1);
            m_device->end_command_list(*cmd_list);
            cmd_list_submit_infos[batch_index] = { *cmd_list };

            batch_index++;
            last_batch_access = task_batch.execution_access;
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
            for (SemaphoreSubmitInfo &sema : info.wait_semas)
                all_wait_semas[i++] = sema;
            for (SemaphoreSubmitInfo &sema : queue_wait_semas)
                all_wait_semas[i++] = sema;
        }

        if (submit_index == m_submits.size() - 1) {
            all_signal_semas = stack.alloc<SemaphoreSubmitInfo>(info.signal_semas.size() + count_of(queue_signal_semas));
            usize i = 0;
            for (SemaphoreSubmitInfo &sema : info.signal_semas)
                all_signal_semas[i++] = sema;
            for (SemaphoreSubmitInfo &sema : queue_signal_semas)
                all_signal_semas[i++] = sema;
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

TaskSubmit &TaskGraph::new_submit(CommandType type)
{
    ZoneScoped;

    u32 submit_id = m_submits.size();
    TaskSubmit &submit = m_submits.emplace_back();

    std::string name = fmt::format("TG Submit {}", submit_id);
    submit.type = type;
    m_device->create_command_allocators(submit.command_allocators, { .type = type, .debug_name = name });
    m_device->create_timestamp_query_pools(submit.query_pools, { .query_count = 128, .debug_name = name });

    return submit;
}

}  // namespace lr::graphics