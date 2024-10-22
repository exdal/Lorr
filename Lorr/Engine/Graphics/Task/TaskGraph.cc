#include "TaskGraph.hh"

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Memory/Stack.hh"

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

bool TaskGraph::init(this TaskGraph &self, Device device_) {
    ZoneScoped;

    self.device = device_;
    u32 frame_count = self.device.frame_count();

    for (u32 i = 0; i < frame_count; i++) {
        self.task_query_pools.emplace_back(QueryPool::create(self.device, 512).value());
    }

    self.new_submit(vk::CommandType::Graphics);

    return true;
}

TaskImageID TaskGraph::add_image(this TaskGraph &self, const TaskImageInfo &info) {
    ZoneScoped;

    TaskImageID task_image_id = static_cast<TaskImageID>(self.images.size());
    self.images.push_back(
        { .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access });

    return task_image_id;
}

void TaskGraph::set_image(this TaskGraph &self, TaskImageID task_image_id, const TaskImageInfo &info) {
    ZoneScoped;

    usize index = static_cast<usize>(task_image_id);
    self.images[index] = {
        .image_id = info.image_id, .image_view_id = info.image_view_id, .last_layout = info.layout, .last_access = info.access
    };
}

TaskBufferID TaskGraph::add_buffer(this TaskGraph &self, const TaskBufferInfo &info) {
    ZoneScoped;

    TaskBufferID task_buffer_id = static_cast<TaskBufferID>(self.buffers.size());
    self.buffers.push_back({ .buffer_id = info.buffer_id });

    return task_buffer_id;
}

TaskID TaskGraph::add_task(this TaskGraph &self, std::unique_ptr<Task> &&task) {
    ZoneScoped;

    TaskID task_id = static_cast<TaskID>(self.tasks.size());

    auto submit_index = self.submits.size() - 1;
    auto batch_index = self.batches.size();
    auto &submit = self.submits[submit_index];
    auto &batch = self.batches.emplace_back();

    for_each_use(
        task->task_uses,
        [&](TaskBufferID id, const vk::PipelineAccessImpl &use_access) {
            auto &task_buffer = self.task_buffer_at(id);
            bool is_read_on_read = use_access.access == vk::MemoryAccess::Read && task_buffer.last_access.access == vk::MemoryAccess::Read;

            if (!is_read_on_read) {
                u32 barrier_id = self.barriers.size();
                self.barriers.push_back(TaskBarrier{
                    .src_access = task_buffer.last_access,
                    .dst_access = use_access,
                });

                batch.barrier_indices.push_back(barrier_id);
            }

            task_buffer.last_access = use_access;
        },
        [&](TaskImageID id, const vk::PipelineAccessImpl &use_access, vk::ImageLayout use_layout) {
            auto &task_image = self.task_image_at(id);
            bool is_same_layout = use_layout == task_image.last_layout;
            bool is_read_on_read = use_access.access == vk::MemoryAccess::Read && task_image.last_access.access == vk::MemoryAccess::Read;

            if (!is_same_layout || !is_read_on_read) {
                u32 barrier_id = self.barriers.size();
                self.barriers.push_back(TaskBarrier{
                    .src_layout = task_image.last_layout,
                    .dst_layout = use_layout,
                    .src_access = task_image.last_access,
                    .dst_access = use_access,
                    .image_id = id,
                });

                batch.barrier_indices.push_back(barrier_id);
            }

            task_image.last_layout = use_layout;
            task_image.last_access = use_access;
        });

    submit.batch_indices.emplace_back(batch_index);
    self.tasks.push_back(std::move(task));

    return task_id;
}

void TaskGraph::present(this TaskGraph &self, TaskImageID task_image_id) {
    ZoneScoped;

    auto &task_image = self.task_image_at(task_image_id);
    auto &task_submit = self.submits[task_image.last_submit_index];

    u32 barrier_id = self.barriers.size();
    self.barriers.push_back({
        .src_layout = task_image.last_layout,
        .dst_layout = vk::ImageLayout::Present,
        .src_access = task_image.last_access,
        .dst_access = vk::PipelineAccess::BottomOfPipe,
        .image_id = task_image_id,
    });
    task_submit.additional_signal_barrier_indices.push_back(barrier_id);
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
            for (u32 batch_index = 0; batch_index < submit.batch_indices.size(); batch_index++) {
                TaskBatch &batch = self.batches[batch_index];

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

    auto &task_query_pool = self.task_query_pools[info.frame_index];
    auto task_query_results = stack.alloc<u64>(self.tasks.size() * 4);

    task_query_pool.get_results(0, self.tasks.size() * 2, task_query_results);

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
            .name = task->name,
            .color = legit::Colors::colors[(task_index * 3 + 1) % count_of(legit::Colors::colors)],
        });

        task_query_offset_ts += delta;
    }

    u32 submit_index = 0;
    for (auto &task_submit : self.submits) {
        auto submit_queue = self.device.queue(task_submit.type);
        auto batch_query_pool = task_submit.query_pools.at(info.frame_index);
        bool is_first_submit = submit_index == 0;
        bool is_last_submit = submit_index == self.submits.size() - 1;

        auto batch_query_results = stack.alloc<u64>(task_submit.batch_indices.size() * 4);
        batch_query_pool.get_results(0, task_submit.batch_indices.size() * 2, batch_query_results);

        for (u32 i = 0; i < batch_query_results.size(); i += 4) {
            if (batch_query_results[i + 1] == 0 && batch_query_results[i + 3] == 0) {
                continue;
            }

            u32 batch_index = i / 4;
            auto &batch = self.batches[batch_index];
            batch.start_ts = static_cast<f64>(batch_query_results[i + 0]);
            batch.end_ts = static_cast<f64>(batch_query_results[i + 2]);
        }

        vk::PipelineAccessImpl last_batch_access = vk::PipelineAccess::TopOfPipe;
        for (auto batch_index : task_submit.batch_indices) {
            auto &task_batch = self.batches[batch_index];
            auto batch_cmd_list = submit_queue.begin();
            CommandBatcher cmd_batcher(batch_cmd_list);

            batch_cmd_list.reset_query_pool(batch_query_pool, batch_index * 2, 2);
            batch_cmd_list.write_query_pool(batch_query_pool, vk::PipelineStage::TopOfPipe, batch_index * 2);

            cmd_batcher.insert_memory_barrier({
                .src_stage = last_batch_access.stage,
                .src_access = last_batch_access.access,
                .dst_stage = task_batch.execution_access.stage,
                .dst_access = task_batch.execution_access.access,
            });

            for (u32 barrier_index : task_batch.barrier_indices) {
                auto &barrier = self.barriers[barrier_index];
                auto &barrier_task_image = self.task_image_at(barrier.image_id);

                cmd_batcher.insert_image_barrier({
                    .src_stage = barrier.src_access.stage,
                    .src_access = barrier.src_access.access,
                    .dst_stage = barrier.dst_access.stage,
                    .dst_access = barrier.dst_access.access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                    .subresource_range = barrier_task_image.subresource_range,
                });
            }
            cmd_batcher.flush_barriers();

            for (TaskID task_id : task_batch.tasks) {
                ZoneScoped;

                u32 task_index = static_cast<u32>(task_id);
                Task &task = *self.tasks[task_index];

                batch_cmd_list.reset_query_pool(task_query_pool, task_index * 2, 2);
                batch_cmd_list.write_query_pool(task_query_pool, vk::PipelineStage::TopOfPipe, task_index * 2);

                TaskContext tc(self, task, batch_cmd_list, info.execution_data);
                task.execute(tc);

                batch_cmd_list.write_query_pool(task_query_pool, vk::PipelineStage::BottomOfPipe, task_index * 2 + 1);
            }

            batch_cmd_list.write_query_pool(batch_query_pool, vk::PipelineStage::BottomOfPipe, batch_index * 2 + 1);
            submit_queue.end(batch_cmd_list);

            batch_index++;
            last_batch_access = task_batch.execution_access;
        }

        bool has_additional_barriers = !task_submit.additional_signal_barrier_indices.empty();
        if (has_additional_barriers) {
            auto cmd_list = submit_queue.begin();
            CommandBatcher cmd_batcher(cmd_list);

            for (u32 barrier_index : task_submit.additional_signal_barrier_indices) {
                auto &barrier = self.barriers[barrier_index];
                auto &barrier_task_image = self.images[static_cast<usize>(barrier.image_id)];
                cmd_batcher.insert_image_barrier({
                    .src_stage = barrier.src_access.stage,
                    .src_access = barrier.src_access.access,
                    .dst_stage = barrier.dst_access.stage,
                    .dst_access = barrier.dst_access.access,
                    .old_layout = barrier.src_layout,
                    .new_layout = barrier.dst_layout,
                    .image_id = barrier_task_image.image_id,
                });
            }
            cmd_batcher.flush_barriers();

            submit_queue.end(cmd_list);
        }

        ls::span<Semaphore> wait_semas_all = {};
        if (is_first_submit) {
            wait_semas_all = info.wait_semas;
        }

        ls::span<Semaphore> signal_semas_all = {};
        if (is_last_submit) {
            signal_semas_all = info.signal_semas;
        }

        submit_queue.submit(wait_semas_all, signal_semas_all);
        submit_index++;
    }
}

TaskSubmit &TaskGraph::new_submit(this TaskGraph &self, vk::CommandType type) {
    ZoneScoped;

    u32 submit_id = self.submits.size();
    TaskSubmit &submit = self.submits.emplace_back();

    std::string name = std::format("TG Submit {}", submit_id);
    submit.type = type;
    for (auto &v : submit.query_pools) {
        v = QueryPool::create(self.device, 128).value();
        v.set_name(name);
    }

    return submit;
}

TaskContext::TaskContext(TaskGraph &task_graph_, Task &task_, CommandList &cmd_list_, void *execution_data_)
    : device(task_graph_.device),
      task_graph(task_graph_),
      task(task_),
      cmd_list(cmd_list_),
      execution_data(execution_data_),
      render_extent({}) {
    glm::uvec2 render_extent_ = {};
    for_each_image_use(task_.task_uses, [&](TaskImageID id, const vk::PipelineAccessImpl &, vk::ImageLayout) {
        auto &task_image = task_graph_.task_image_at(id);
        auto image = device.image(task_image.image_id);
        auto extent = image.extent();
        render_extent_ = glm::min(render_extent_, { extent.width, extent.height });
    });

    render_extent = { render_extent_.x, render_extent_.y };
}

vk::RenderingAttachmentInfo TaskContext::as_color_attachment(
    this TaskContext &self, TaskUse &use, std::optional<vk::ColorClearValue> clear_val) {
    ZoneScoped;

    vk::RenderingAttachmentInfo info = {
        .image_view_id = self.task_image_data(use).image_view_id,
        .image_layout = vk::ImageLayout::ColorAttachment,
        .load_op = clear_val ? vk::AttachmentLoadOp::Clear : vk::AttachmentLoadOp::Load,
        .store_op = use.access.access & vk::MemoryAccess::Write ? vk::AttachmentStoreOp::Store : vk::AttachmentStoreOp::None,
        .clear_value = { .color = clear_val.value_or(vk::ColorClearValue{}) },
    };

    return info;
}

vk::RenderingAttachmentInfo TaskContext::as_depth_attachment(
    this TaskContext &self, TaskUse &use, std::optional<vk::DepthClearValue> clear_val) {
    ZoneScoped;

    vk::RenderingAttachmentInfo info = {
        .image_view_id = self.task_image_data(use).image_view_id,
        .image_layout = vk::ImageLayout::DepthStencilAttachment,
        .load_op = clear_val ? vk::AttachmentLoadOp::Clear : vk::AttachmentLoadOp::Load,
        .store_op = use.access.access & vk::MemoryAccess::Write ? vk::AttachmentStoreOp::Store : vk::AttachmentStoreOp::None,
        .clear_value = { .depth = clear_val.value_or(vk::DepthClearValue{}) },
    };

    return info;
}

TaskImage &TaskContext::task_image_data(this TaskContext &self, TaskUse &use) {
    return self.task_graph.task_image_at(use.task_image_id);
}

TaskImage &TaskContext::task_image_data(this TaskContext &self, TaskImageID task_image_id) {
    return self.task_graph.task_image_at(task_image_id);
}

vk::Viewport TaskContext::pass_viewport(this TaskContext &self) {
    return {
        .x = 0,
        .y = 0,
        .width = static_cast<f32>(self.render_extent.width),
        .height = static_cast<f32>(self.render_extent.height),
        .depth_min = 0.0f,
        .depth_max = 1.0f,
    };
}

vk::Rect2D TaskContext::pass_rect(this TaskContext &self) {
    return {
        .offset = { 0, 0 },
        .extent = self.render_extent,
    };
}

void TaskContext::set_pipeline(this TaskContext &self, PipelineID pipeline_id) {
    ZoneScoped;

    auto pipeline = self.device.pipeline(pipeline_id);
    self.cmd_list.set_pipeline(pipeline_id);
    self.cmd_list.set_descriptor_set();

    self.pipeline_id = pipeline_id;
    self.pipeline_layout_id = pipeline.layout_id();
}

}  // namespace lr
