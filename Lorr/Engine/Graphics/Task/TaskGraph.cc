#include "TaskGraph.hh"

#include "Engine/Graphics/Vulkan.hh"
#include "Engine/Memory/Stack.hh"
#include "Engine/Util/LegitProfiler.hh"

namespace lr {
void for_each_use(ls::span<TaskUse> uses, const auto &image_fn) {
    for (TaskUse &use : uses) {
        image_fn(use.task_image_id, use.access, use.image_layout);
    }
}

template<>
struct Handle<TaskGraph>::Impl {
    Device device = {};

    TransferManager transfer_man = {};
    std::vector<std::unique_ptr<Task>> tasks = {};
    std::vector<TaskSubmit> submits = {};
    std::vector<TaskBatch> batches = {};
    std::vector<TaskBarrier> barriers = {};

    std::vector<TaskImage> images = {};

    // Profilers
    ls::static_vector<QueryPool, Device::Limits::FrameCount> task_query_pools = {};
    std::vector<legit::ProfilerTask> task_gpu_profiler_tasks = {};
    legit::ProfilerGraph task_gpu_profiler_graph = { 400 };
};

auto TaskGraph::create(const TaskGraphInfo &info) -> TaskGraph {
    ZoneScoped;

    auto impl = new Impl;
    impl->device = info.device;

    for (u32 i = 0; i < info.device.frame_count(); i++) {
        impl->task_query_pools.emplace_back(QueryPool::create(info.device, 512).value());
    }

    impl->transfer_man = TransferManager::create(info.device, info.staging_buffer_size, info.staging_buffer_uses);
    auto tg = TaskGraph(impl);
    tg.new_submit(vk::CommandType::Graphics);

    return tg;
}

auto TaskGraph::add_image(const TaskImageInfo &info) -> TaskImageID {
    ZoneScoped;

    auto task_image_id = static_cast<TaskImageID>(impl->images.size());
    impl->images.push_back({
        .image_id = info.image_id,
        .last_layout = info.layout,
        .last_access = info.access,
    });

    return task_image_id;
}

auto TaskGraph::set_image(TaskImageID task_image_id, const TaskImageInfo &info) -> void {
    ZoneScoped;

    auto index = static_cast<usize>(task_image_id);
    impl->images[index] = {
        .image_id = info.image_id,
        .last_layout = info.layout,
        .last_access = info.access,
    };
}

auto TaskGraph::new_submit(vk::CommandType type) -> usize {
    ZoneScoped;

    auto submit_id = impl->submits.size();
    auto &submit = impl->submits.emplace_back();

    auto name = std::format("TG Submit {}", submit_id);
    submit.type = type;
    for (auto &v : submit.query_pools) {
        v = QueryPool::create(impl->device, 128).value();
        v.set_name(name);
    }

    return submit_id;
}

auto TaskGraph::add_task(std::unique_ptr<Task> &&task) -> TaskID {
    ZoneScoped;

    auto task_id = static_cast<TaskID>(impl->tasks.size());

    auto submit_index = impl->submits.size() - 1;
    auto batch_index = impl->batches.size();
    auto &submit = impl->submits[submit_index];
    auto &batch = impl->batches.emplace_back();

    for_each_use(task->task_uses, [&](TaskImageID id, const vk::PipelineAccessImpl &use_access, vk::ImageLayout use_layout) {
        auto &task_image = this->task_image(id);
        bool is_same_layout = use_layout == task_image.last_layout;
        bool is_read_on_read = use_access.access == vk::MemoryAccess::Read && task_image.last_access.access == vk::MemoryAccess::Read;

        if (!is_same_layout || !is_read_on_read) {
            u32 barrier_id = impl->barriers.size();
            impl->barriers.push_back(TaskBarrier{
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
    batch.tasks.push_back(task_id);
    impl->tasks.push_back(std::move(task));

    return task_id;
}

auto TaskGraph::present(TaskImageID dst_task_image_id) -> void {
    ZoneScoped;

    auto &task_image = this->task_image(dst_task_image_id);
    auto &task_submit = impl->submits[task_image.last_submit_index];

    u32 barrier_id = impl->barriers.size();
    impl->barriers.push_back({
        .src_layout = task_image.last_layout,
        .dst_layout = vk::ImageLayout::Present,
        .src_access = task_image.last_access,
        .dst_access = vk::PipelineAccess::BottomOfPipe,
        .image_id = dst_task_image_id,
    });
    task_submit.additional_signal_barrier_indices.push_back(barrier_id);
}

auto TaskGraph::draw_profiler_ui() -> void {
    ZoneScoped;

    auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(ImVec2(750, 450), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Legit Profiler", nullptr);
    ImGui::Text("Frametime: %.4fms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    impl->task_gpu_profiler_graph.LoadFrameData(impl->task_gpu_profiler_tasks.data(), impl->task_gpu_profiler_tasks.size());
    impl->task_gpu_profiler_graph.RenderTimings(500, 20, 200, 0);
    ImGui::SliderFloat("Graph detail", &impl->task_gpu_profiler_graph.maxFrameTime, 1.0f, 10000.f);

    ImGui::Separator();

    for (u32 submit_index = 0; submit_index < impl->submits.size(); submit_index++) {
        TaskSubmit &submit = impl->submits[submit_index];

        if (ImGui::TreeNode(&submit, "Submit %u", submit_index)) {
            for (u32 batch_index = 0; batch_index < submit.batch_indices.size(); batch_index++) {
                TaskBatch &batch = impl->batches[batch_index];

                if (ImGui::TreeNode(&batch, "Batch %u - %lfms", batch_index, batch.execution_time())) {
                    ImGui::Text("%lu total barrier(s).", batch.barrier_indices.size());
                    ImGui::Text("%lu total task(s).", batch.tasks.size());

                    for (TaskID task_id : batch.tasks) {
                        Task *task = &*impl->tasks[static_cast<usize>(task_id)];
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

auto TaskGraph::execute(const TaskExecuteInfo &info) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    impl->task_gpu_profiler_tasks.clear();

    auto &task_query_pool = impl->task_query_pools[info.frame_index];
    auto task_query_results = stack.alloc<u64>(impl->tasks.size() * 4);

    task_query_pool.get_results(0, impl->tasks.size() * 2, task_query_results);

    f64 task_query_offset_ts = 0.0;
    for (u32 i = 0; i < task_query_results.size(); i += 4) {
        if (task_query_results[i + 1] == 0 && task_query_results[i + 3] == 0) {
            continue;
        }

        u32 task_index = i / 4;
        auto &task = impl->tasks[task_index];
        task->start_ts = static_cast<f64>(task_query_results[i + 0]);
        task->end_ts = static_cast<f64>(task_query_results[i + 2]);

        f64 delta = ((task->end_ts / 1e6f) - (task->start_ts / 1e6f)) / 1e3f;
        impl->task_gpu_profiler_tasks.push_back({
            .startTime = task_query_offset_ts,
            .endTime = task_query_offset_ts + delta,
            .name = task->name,
            .color = legit::Colors::colors[(task_index * 3 + 1) % count_of(legit::Colors::colors)],
        });

        task_query_offset_ts += delta;
    }

    u32 submit_index = 0;
    for (auto &task_submit : impl->submits) {
        auto submit_queue = impl->device.queue(task_submit.type);
        auto submit_sema = submit_queue.semaphore();
        auto batch_query_pool = task_submit.query_pools.at(info.frame_index);
        bool is_first_submit = submit_index == 0;
        bool is_last_submit = submit_index == impl->submits.size() - 1;

        auto batch_query_results = stack.alloc<u64>(task_submit.batch_indices.size() * 4);
        batch_query_pool.get_results(0, task_submit.batch_indices.size() * 2, batch_query_results);

        for (u32 i = 0; i < batch_query_results.size(); i += 4) {
            if (batch_query_results[i + 1] == 0 && batch_query_results[i + 3] == 0) {
                continue;
            }

            u32 batch_index = i / 4;
            auto &batch = impl->batches[batch_index];
            batch.start_ts = static_cast<f64>(batch_query_results[i + 0]);
            batch.end_ts = static_cast<f64>(batch_query_results[i + 2]);
        }

        vk::PipelineAccessImpl last_batch_access = vk::PipelineAccess::TopOfPipe;
        for (auto batch_index : task_submit.batch_indices) {
            auto &task_batch = impl->batches[batch_index];
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
                auto &barrier = impl->barriers[barrier_index];
                auto &barrier_task_image = this->task_image(barrier.image_id);

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

                auto &task = this->task(task_id);

                auto task_index = static_cast<u32>(task_id);
                batch_cmd_list.reset_query_pool(task_query_pool, task_index * 2, 2);
                batch_cmd_list.write_query_pool(task_query_pool, vk::PipelineStage::TopOfPipe, task_index * 2);

                TaskContext tc(impl->device, *this, task, batch_cmd_list, info.execution_data);
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
                auto &barrier = impl->barriers[barrier_index];
                auto &barrier_task_image = impl->images[static_cast<usize>(barrier.image_id)];
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

        ls::span<Semaphore> wait_semas_all = submit_sema;
        if (false) {
            wait_semas_all = stack.alloc<Semaphore>(info.wait_semas.size() + 1);
            u32 i = 0;
            for (; i < info.wait_semas.size(); i++) {
                wait_semas_all[i] = info.wait_semas[i];
            }

            wait_semas_all[i++] = submit_sema;
        }

        ls::span<Semaphore> signal_semas_all = submit_sema;
        if (is_last_submit) {
            signal_semas_all = stack.alloc<Semaphore>(info.signal_semas.size() + 2);
            u32 i = 0;
            for (; i < info.signal_semas.size(); i++) {
                signal_semas_all[i] = info.signal_semas[i];
            }

            signal_semas_all[i++] = submit_sema;
            signal_semas_all[i++] = impl->transfer_man.semaphore();
        }

        submit_queue.submit(wait_semas_all, signal_semas_all);
        submit_index++;
    }

    impl->transfer_man.collect_garbage();
}

auto TaskGraph::transfer_manager() -> TransferManager & {
    return impl->transfer_man;
}

auto TaskGraph::task(TaskID id) -> Task & {
    return *impl->tasks.at(static_cast<usize>(id)).get();
}

auto TaskGraph::task_image(TaskImageID id) -> TaskImage & {
    return impl->images.at(static_cast<usize>(id));
}

TaskContext::TaskContext(Device &device_, TaskGraph &task_graph_, Task &task_, CommandList &cmd_list_, void *execution_data_)
    : device(device_),
      task_graph(task_graph_),
      task(task_),
      cmd_list(cmd_list_),
      execution_data(execution_data_),
      render_extent({}) {
    glm::uvec2 render_extent_ = { ~0_u32, ~0_u32 };
    for_each_use(task_.task_uses, [&](TaskImageID id, const vk::PipelineAccessImpl &, vk::ImageLayout) {
        auto &task_image = task_graph_.task_image(id);
        auto image = device.image(task_image.image_id);
        auto extent = image.extent();
        render_extent_ = glm::min(render_extent_, { extent.width, extent.height });
    });

    render_extent = { render_extent_.x, render_extent_.y };
}

vk::RenderingAttachmentInfo TaskContext::as_color_attachment(
    this TaskContext &self, TaskUse &use, std::optional<vk::ColorClearValue> clear_val) {
    ZoneScoped;

    auto &task_image = self.task_image_data(use);
    vk::RenderingAttachmentInfo info = {
        .image_view_id = self.device.image(task_image.image_id).view(),
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

    auto &task_image = self.task_image_data(use);
    vk::RenderingAttachmentInfo info = {
        .image_view_id = self.device.image(task_image.image_id).view(),
        .image_layout = vk::ImageLayout::DepthStencilAttachment,
        .load_op = clear_val ? vk::AttachmentLoadOp::Clear : vk::AttachmentLoadOp::Load,
        .store_op = use.access.access & vk::MemoryAccess::Write ? vk::AttachmentStoreOp::Store : vk::AttachmentStoreOp::None,
        .clear_value = { .depth = clear_val.value_or(vk::DepthClearValue{}) },
    };

    return info;
}

TaskUse &TaskContext::task_use(usize index) {
    return this->task.task_uses[index];
}

TaskImage &TaskContext::task_image_data(this TaskContext &self, TaskUse &use) {
    return self.task_graph.task_image(use.task_image_id);
}

TaskImage &TaskContext::task_image_data(this TaskContext &self, TaskImageID task_image_id) {
    return self.task_graph.task_image(task_image_id);
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

GPUAllocation TaskContext::transient_buffer(u64 size) {
    ZoneScoped;

    return this->task_graph.transfer_manager().allocate(size).value();
}

ImageViewID TaskContext::image_view(TaskImageID id) {
    ZoneScoped;

    return this->device.image(this->task_image_data(id).image_id).view();
}

void TaskContext::set_pipeline(this TaskContext &self, PipelineID pipeline_id_) {
    ZoneScoped;

    auto pipeline = self.device.pipeline(pipeline_id_);
    self.cmd_list.set_pipeline(pipeline_id_);
    self.cmd_list.set_descriptor_set();

    self.pipeline_id = pipeline_id_;
    self.pipeline_layout_id = pipeline.layout_id();
}

}  // namespace lr
