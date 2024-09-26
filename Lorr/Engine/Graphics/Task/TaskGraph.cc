#include "TaskGraph.hh"

#include "Engine/Graphics/Device.hh"

#include "Engine/Memory/Stack.hh"

#include <sstream>

namespace lr {
void for_each_use(ls::span<TaskUse> uses, const auto &buffer_fn, const auto &image_fn) {
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

void for_each_image_use(ls::span<TaskUse> uses, const auto &image_fn) {
    for (TaskUse &use : uses) {
        if (use.type == TaskUseType::Image) {
            image_fn(use.task_image_id, use.access, use.image_layout);
        }
    }
}

bool TaskGraph::init(this TaskGraph &self, const TaskGraphInfo &info) {
    ZoneScoped;

    self.device = info.device;
    u32 frame_count = self.device->frame_count;

    self.task_query_pools.resize(frame_count);
    self.device->create_timestamp_query_pools(self.task_query_pools, { .query_count = 512, .debug_name = "TG Task Query Pool" });

    self.new_submit(CommandType::Graphics);

    return true;
}

TaskImageID TaskGraph::add_image(this TaskGraph &self, const TaskPersistentImageInfo &info) {
    ZoneScoped;

    TaskImageID task_image_id = static_cast<TaskImageID>(self.images.size());
    self.images.push_back({ .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access });

    return task_image_id;
}

void TaskGraph::set_image(this TaskGraph &self, TaskImageID task_image_id, const TaskPersistentImageInfo &info) {
    ZoneScoped;

    usize index = static_cast<usize>(task_image_id);
    self.images[index] = { .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access };
}

TaskBufferID TaskGraph::add_buffer(this TaskGraph &self, const TaskBufferInfo &info) {
    ZoneScoped;

    TaskBufferID task_buffer_id = static_cast<TaskBufferID>(self.buffers.size());
    self.buffers.push_back({ .buffer_id = info.buffer_id });

    return task_buffer_id;
}

void TaskGraph::update_task_infos(this TaskGraph &self) {
    ZoneScoped;

    for (auto &task : self.tasks) {
        glm::uvec2 render_extent = {};
        for_each_image_use(task->task_uses, [&](TaskImageID id, const PipelineAccessImpl &, ImageLayout) {
            TaskImage &task_image = self.images[static_cast<usize>(id)];
            Image &image = self.device->image_at(task_image.image_id);
            render_extent = glm::min(render_extent, glm::uvec2({ image.extent.width, image.extent.height }));
        });

        task->render_extent = { render_extent.x, render_extent.y };
    }
}

u32 TaskGraph::schedule_task(this TaskGraph &self, Task *task, TaskSubmit &submit) {
    ZoneScoped;

    u32 first_batch_index = 0;
    for_each_use(
        task->task_uses,
        [&](TaskBufferID id, const PipelineAccessImpl &access) {
            TaskBufferInfo &task_buffer = self.buffers[static_cast<usize>(id)];
            u32 cur_batch_index = task_buffer.last_batch_index;

            bool is_read_on_read = access.access == MemoryAccess::Read && task_buffer.last_access.access == MemoryAccess::Read;
            if (!is_read_on_read) {
                cur_batch_index++;
            }

            first_batch_index = std::max(first_batch_index, cur_batch_index);
        },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = self.images[static_cast<usize>(id)];
            auto &last_access = task_image.last_access;
            u32 cur_batch_index = task_image.last_batch_index;

            bool is_same_layout = layout == task_image.last_layout;
            bool is_read_on_read = access.access == MemoryAccess::Read && last_access.access == MemoryAccess::Read;

            bool is_same_layout_read_on_read = is_same_layout && is_read_on_read;

            if (is_same_layout_read_on_read) {
                cur_batch_index++;
            }

            first_batch_index = std::max(first_batch_index, cur_batch_index);
        });

    if (first_batch_index >= submit.batches.size()) {
        submit.batches.resize(first_batch_index + 1);
    }

    return first_batch_index;
}

TaskID TaskGraph::add_task(this TaskGraph &self, std::unique_ptr<Task> &&task) {
    ZoneScoped;

    if (!self.prepare_task(*task)) {
        LOG_ERROR("Failed to prepare task '{}'!", task->name);
        return TaskID::Invalid;
    }

    TaskID task_id = static_cast<TaskID>(self.tasks.size());
    u32 submit_index = self.submits.size() - 1;
    TaskSubmit &current_submit = self.submits[submit_index];
    u32 batch_index = self.schedule_task(&*task, current_submit);
    TaskBatch &batch = current_submit.batches[batch_index];

    for_each_use(
        task->task_uses,
        [&](TaskBufferID id, const PipelineAccessImpl &access) {
            TaskBufferInfo &task_buffer = self.buffers[static_cast<usize>(id)];
            bool is_read_on_read = access.access == MemoryAccess::Read && task_buffer.last_access.access == MemoryAccess::Read;
            if (!is_read_on_read) {
                batch.execution_access |= access;
            }

            task_buffer.last_access = access;
            task_buffer.last_batch_index = batch_index;
            task_buffer.last_submit_index = submit_index;
        },
        [&](TaskImageID id, const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = self.images[static_cast<usize>(id)];
            bool is_same_layout = layout == task_image.last_layout;
            bool is_read_on_read = access.access == MemoryAccess::Read && task_image.last_access.access == MemoryAccess::Read;

            if (!is_same_layout || !is_read_on_read) {
                u32 barrier_id = self.barriers.size();
                self.barriers.push_back(TaskBarrier{
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
    self.tasks.push_back(std::move(task));

    return task_id;
}

void TaskGraph::present(this TaskGraph &self, TaskImageID task_image_id) {
    ZoneScoped;

    TaskImage &task_image = self.images[static_cast<usize>(task_image_id)];
    TaskSubmit &task_submit = self.submits[task_image.last_submit_index];

    u32 barrier_id = self.barriers.size();
    self.barriers.push_back({
        .src_layout = task_image.last_layout,
        .dst_layout = ImageLayout::Present,
        .src_access = task_image.last_access,
        .dst_access = PipelineAccess::BottomOfPipe,
        .image_id = task_image_id,
    });
    task_submit.additional_signal_barrier_indices.push_back(barrier_id);
}

std::string TaskGraph::generate_graphviz(this TaskGraph &self) {
    ZoneScoped;

    u32 batch_id = 0;

    std::stringstream ss;
    ss << "digraph task_graph {\n";
    ss << "graph [rankdir = \"LR\" compound = true];\n";
    ss << "node [fontsize = \"16\" shape = \"ellipse\"]\n";
    ss << "edge [];\n";
    for (u32 task_id = 0; task_id < self.tasks.size(); task_id++) {
        std::string_view name = self.tasks[task_id]->name;
        auto uses = self.tasks[task_id]->task_uses;

        ss << std::format(R"(task{} [)", task_id) << "\n";
        ss << std::format(R"(label = "<name> {}({}))", name, task_id);
        for (auto &use : uses) {
            ss << std::format(R"(|<i{}> image {})", use.index, use.index);
        }
        ss << "\"\n";
        ss << R"(shape = "record")"
           << "\n";
        ss << "];\n";
    }

    // mfw dot doesnt have proper subgraph connectors
    for (TaskSubmit &submit : self.submits) {
        for (u32 i = 0; i < submit.batches.size(); i++) {
            TaskBatch &batch = submit.batches[i];
            TaskID mid_task_id = batch.tasks[batch.tasks.size() / 2];

            ss << std::format(R"(subgraph cluster_{})", batch_id) << " {\n";
            ss << std::format(R"(label = "Batch {}\n{} barriers";)", batch_id, batch.barrier_indices.size()) << "\n";
            if (i != 0) {
                TaskBatch &last_batch = submit.batches[i - 1];
                TaskID last_mid_task_id = last_batch.tasks[last_batch.tasks.size() / 2];
                u32 lmtidx = static_cast<u32>(last_mid_task_id);
                u32 mtidx = static_cast<u32>(mid_task_id);
                ss << std::format("task{} -> task{} [ltail = cluster_{} lhead = cluster_{} label=\"todo barrier info\"];\n", lmtidx, mtidx, i - 1, i);
            }
            for (TaskID task_id : batch.tasks) {
                if (task_id == mid_task_id && i != 0) {
                    continue;
                }

                u32 task_index = static_cast<u32>(task_id);
                ss << std::format("task{} ", task_index);
            }

            ss << "\n}\n";
            batch_id++;
        }
    }

    ss << "}\n";

    return ss.str();
}

void TaskGraph::draw_profiler_ui(this TaskGraph &self) {
    ZoneScoped;

    auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(ImVec2(750, 450), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Legit Profiler", nullptr);
    ImGui::Text("Frametime: %.4fms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    self.task_gpu_profiler_graph.LoadFrameData(self.task_gpu_profiler_tasks.data(), self.task_gpu_profiler_tasks.size());
    self.task_gpu_profiler_graph.RenderTimings(500, 20, 200, 0);
    ImGui::SliderFloat("Graph detail", &self.task_gpu_profiler_graph.maxFrameTime, 1.0f, 10000.f);

    ImGui::Separator();

    for (u32 submit_index = 0; submit_index < self.submits.size(); submit_index++) {
        TaskSubmit &submit = self.submits[submit_index];

        if (ImGui::TreeNode(&submit, "Submit %u", submit_index)) {
            for (u32 batch_index = 0; batch_index < submit.batches.size(); batch_index++) {
                TaskBatch &batch = submit.batches[batch_index];

                if (ImGui::TreeNode(&batch, "Batch %u - %lfms", batch_index, batch.execution_time())) {
                    ImGui::Text("%lu total barrier(s).", batch.barrier_indices.size());
                    ImGui::Text("%lu total task(s).", batch.tasks.size());

                    for (TaskID task_id : batch.tasks) {
                        Task *task = &*self.tasks[static_cast<usize>(task_id)];
                        ImGui::BulletText("%s - %lfms", task->name.data(), task->execution_time());
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void TaskGraph::execute(this TaskGraph &self, const TaskExecuteInfo &info) {
    ZoneScoped;
    memory::ScopedStack stack;

    self.task_gpu_profiler_tasks.clear();

    auto &transfer_queue = self.device->queue_at(CommandType::Transfer);
    auto &transfer_sema = transfer_queue.semaphore;
    auto &task_query_pool = self.task_query_pools[info.frame_index];
    auto task_query_results = stack.alloc<u64>(self.tasks.size() * 4);

    self.device->get_timestamp_query_pool_results(task_query_pool, 0, self.tasks.size() * 2, task_query_results);

    f64 task_query_offset_ts = 0.0;
    for (u32 i = 0; i < task_query_results.size(); i += 4) {
        if (task_query_results[i + 1] == 0 && task_query_results[i + 3] == 0) {
            continue;
        }

        u32 task_index = i / 4;
        auto &task = self.tasks[task_index];
        task->start_ts = static_cast<f64>(task_query_results[i + 0]);
        task->end_ts = static_cast<f64>(task_query_results[i + 2]);

        f64 delta = ((task->end_ts / 1e6f) - (task->start_ts / 1e6f)) / 1e3f;
        self.task_gpu_profiler_tasks.push_back({
            .startTime = task_query_offset_ts,
            .endTime = task_query_offset_ts + delta,
            .name = task->name.data(),
            .color = legit::Colors::colors[(task_index * 3 + 1) % count_of(legit::Colors::colors)],
        });

        task_query_offset_ts += delta;
    }

    u32 submit_index = 0;
    for (auto &task_submit : self.submits) {
        auto &submit_queue = self.device->queue_at(task_submit.type);
        auto &batch_query_pool = task_submit.frame_query_pool(info.frame_index);
        bool is_first_submit = submit_index == 0;
        bool is_last_submit = submit_index == self.submits.size() - 1;

        auto batch_query_results = stack.alloc<u64>(task_submit.batches.size() * 4);
        self.device->get_timestamp_query_pool_results(batch_query_pool, 0, task_submit.batches.size() * 2, batch_query_results);

        for (u32 i = 0; i < batch_query_results.size(); i += 4) {
            if (batch_query_results[i + 1] == 0 && batch_query_results[i + 3] == 0) {
                continue;
            }

            u32 batch_index = i / 4;
            auto &batch = task_submit.batches[batch_index];
            batch.start_ts = static_cast<f64>(batch_query_results[i + 0]);
            batch.end_ts = static_cast<f64>(batch_query_results[i + 2]);
        }

        PipelineAccessImpl last_batch_access = PipelineAccess::TopOfPipe;
        usize batch_index = 0;
        for (auto &task_batch : task_submit.batches) {
            auto &copy_cmd_list = transfer_queue.begin_command_list(info.frame_index);
            auto &batch_cmd_list = submit_queue.begin_command_list(info.frame_index);
            CommandBatcher cmd_batcher(batch_cmd_list);

            batch_cmd_list.reset_query_pool(batch_query_pool, batch_index * 2, 2);
            batch_cmd_list.write_timestamp(batch_query_pool, PipelineStage::TopOfPipe, batch_index * 2);

            cmd_batcher.insert_memory_barrier({
                .src_access = last_batch_access,
                .dst_access = task_batch.execution_access,
            });

            for (u32 barrier_index : task_batch.barrier_indices) {
                auto &barrier = self.barriers[barrier_index];
                auto &barrier_task_image = self.task_image_at(barrier.image_id);

                cmd_batcher.insert_image_barrier({
                    .src_access = barrier.src_access,
                    .dst_access = barrier.dst_access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                    .subresource_range = barrier_task_image.subresource_range,
                });
            }
            cmd_batcher.flush_barriers();

            for (TaskID task_id : task_batch.tasks) {
                u32 task_index = static_cast<u32>(task_id);
                batch_cmd_list.reset_query_pool(task_query_pool, task_index * 2, 2);
                batch_cmd_list.write_timestamp(task_query_pool, PipelineStage::TopOfPipe, task_index * 2);

                Task &task = *self.tasks[task_index];
                if (task.pipeline_id != PipelineID::Invalid) {
                    Pipeline &pipeline = self.device->pipeline_at(task.pipeline_id);
                    batch_cmd_list.set_pipeline(task.pipeline_id);
                    batch_cmd_list.set_descriptor_sets(task.pipeline_layout_id, pipeline.bind_point, 0, self.device->descriptor_set);
                }

                {
                    ZoneScoped;
                    TaskContext tc(*self.device, self, task, batch_cmd_list, copy_cmd_list, info.frame_index, info.execution_data);
                    task.execute(tc);
                }
                batch_cmd_list.write_timestamp(task_query_pool, PipelineStage::BottomOfPipe, task_index * 2 + 1);
            }

            batch_cmd_list.write_timestamp(batch_query_pool, PipelineStage::BottomOfPipe, batch_index * 2 + 1);
            submit_queue.end_command_list(batch_cmd_list);

            // Submit transfer queue
            transfer_queue.end_command_list(copy_cmd_list);
            transfer_queue.submit(info.frame_index, { .self_wait = false });

            batch_index++;
            last_batch_access = task_batch.execution_access;
        }

        bool has_additional_barriers = !task_submit.additional_signal_barrier_indices.empty();
        if (has_additional_barriers) {
            auto &cmd_list = submit_queue.begin_command_list(info.frame_index);
            CommandBatcher cmd_batcher(cmd_list);

            for (u32 barrier_index : task_submit.additional_signal_barrier_indices) {
                auto &barrier = self.barriers[barrier_index];
                auto &barrier_task_image = self.images[static_cast<usize>(barrier.image_id)];
                cmd_batcher.insert_image_barrier({
                    .src_access = barrier.src_access,
                    .dst_access = barrier.dst_access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                });
            }
            cmd_batcher.flush_barriers();

            submit_queue.end_command_list(cmd_list);
        }

        SemaphoreSubmitInfo wait_semas[] = { { transfer_sema, transfer_sema.counter, PipelineStage::AllTransfer } };
        ls::span<SemaphoreSubmitInfo> wait_semas_all = {};
        if (is_first_submit) {
            wait_semas_all = stack.alloc<SemaphoreSubmitInfo>(info.wait_semas.size() + count_of(wait_semas));

            usize i = 0;
            for (auto &v : info.wait_semas) {
                wait_semas_all[i++] = v;
            }

            for (auto &v : wait_semas) {
                wait_semas_all[i++] = v;
            }
        }

        ls::span<SemaphoreSubmitInfo> signal_semas_all = {};
        if (is_last_submit) {
            signal_semas_all = info.signal_semas;
        }

        submit_queue.submit(info.frame_index, { .additional_wait_semas = wait_semas_all, .additional_signal_semas = signal_semas_all });
        submit_index++;
    }
}

bool TaskGraph::prepare_task(this TaskGraph &self, Task &task) {
    ZoneScoped;

    TaskPrepareInfo prepare_info = {
        .device = self.device,
    };

    // it is okay to resize to uses size despite Task::Uses contains buffers
    // as long as GraphicsPipelineInfo::Formats is less than/eqals to resize size
    prepare_info.pipeline_info.blend_attachments.resize(task.task_uses.size());

    if (task.prepare(prepare_info)) {
        std::vector<Format> color_attachment_formats = {};

        auto &pipeline_info = prepare_info.pipeline_info;
        auto &graphics_info = pipeline_info.graphics_info;
        auto &compute_info = pipeline_info.compute_info;

        glm::uvec2 render_extent = { ~0_u32, ~0_u32 };
        for_each_image_use(task.task_uses, [&](TaskImageID id, [[maybe_unused]] const PipelineAccessImpl &access, ImageLayout layout) {
            TaskImage &task_image = self.images[static_cast<usize>(id)];
            LS_EXPECT(task_image.image_id != ImageID::Invalid);
            Image &image = self.device->image_at(task_image.image_id);

            switch (layout) {
                case ImageLayout::ColorAttachment:
                    color_attachment_formats.push_back(image.format);
                    break;
                case ImageLayout::DepthStencilAttachment:
                    graphics_info.depth_attachment_format = image.format;
                    graphics_info.stencil_attachment_format = image.format;
                    break;
                case ImageLayout::DepthAttachment:
                    graphics_info.depth_attachment_format = image.format;
                    break;
                case ImageLayout::StencilAttachment:
                    graphics_info.stencil_attachment_format = image.format;
                    break;
                default:
                    return;
            }

            render_extent = glm::min(render_extent, glm::uvec2({ image.extent.width, image.extent.height }));
        });

        task.render_extent = { render_extent.x, render_extent.y };
        usize color_attachment_size = color_attachment_formats.size();
        bool is_graphics_pipeline =
            !color_attachment_formats.empty() || graphics_info.depth_attachment_format != Format::Unknown || graphics_info.stencil_attachment_format != Format::Unknown;

        if (is_graphics_pipeline) {
            LS_EXPECT(!pipeline_info.shader_ids.empty());

            graphics_info.color_attachment_formats = color_attachment_formats;
            graphics_info.viewports = pipeline_info.viewports;
            graphics_info.scissors = pipeline_info.scissors;
            graphics_info.vertex_binding_infos = pipeline_info.vertex_binding_infos;
            graphics_info.vertex_attrib_infos = pipeline_info.vertex_attrib_infos;
            graphics_info.blend_attachments = { pipeline_info.blend_attachments.data(), color_attachment_size };
            graphics_info.shader_ids = pipeline_info.shader_ids;
            graphics_info.layout_id = task.pipeline_layout_id;

            task.pipeline_id = self.device->create_graphics_pipeline(graphics_info);
        } else {
            LS_EXPECT(pipeline_info.shader_ids.size() == 1);
            compute_info.shader_id = pipeline_info.shader_ids[0];
            compute_info.layout_id = task.pipeline_layout_id;

            task.pipeline_id = self.device->create_compute_pipeline(compute_info);
        }
    }

    // TODO: probably should add some error checking instead of using bools
    // this should return 3rd value indicating that this pass is probably a
    // transfer or a dummy.
    return true;
}

TaskSubmit &TaskGraph::new_submit(this TaskGraph &self, CommandType type) {
    ZoneScoped;

    u32 submit_id = self.submits.size();
    TaskSubmit &submit = self.submits.emplace_back();

    std::string name = std::format("TG Submit {}", submit_id);
    submit.type = type;
    self.device->create_timestamp_query_pools(submit.query_pools, { .query_count = 128, .debug_name = name });

    return submit;
}

}  // namespace lr
