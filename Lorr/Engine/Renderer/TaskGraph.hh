#pragma once

#include "Graphics/SwapChain.hh"
#include "PipelineManager.hh"
#include "Task.hh"
#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"
#include "Graphics/MemoryAllocator.hh"
#include "Memory/Allocator/AreaAllocator.hh"

#include <EASTL/queue.h>
#include <EASTL/vector.h>

namespace lr::Graphics {
struct TaskFrame {
    constexpr static usize k_type_count = static_cast<usize>(CommandType::Count);

    Semaphore m_timeline_sema = {};
    eastl::array<CommandAllocator, k_type_count> m_allocators = {};
    PoolID m_frametime_memory = PoolID::Invalid;
    eastl::queue<BufferID> m_transient_buffers = {};

    DescriptorSet m_descriptor_set = {};
};

struct TaskPresentDesc {
    TaskImageID m_swap_chain_image_id = TaskImageID::Invalid;
};

struct TaskExecuteDesc {
    TaskImageID m_swap_chain_image_id = TaskImageID::Invalid;
};

struct TaskGraphDesc {
    Device *m_device = nullptr;
    SwapChain *m_swap_chain = nullptr;
};

struct TaskGraph {
    void init(TaskGraphDesc *desc);
    ~TaskGraph();

    TaskImageID use_persistent_image(const PersistentImageInfo &persistent_image_info);
    void set_image(TaskImageID task_image_id, ImageID image_id, ImageViewID image_view_id);

    TaskBufferID use_persistent_buffer(const PersistentBufferInfo &buffer_info);

    TaskBatchID schedule_task(Task &task);
    template<typename TaskT>
    void add_task(const TaskT &task_info);
    void add_task(Task &task, TaskID id);
    void compile_task_pipeline(Task &task);

    // Record operations
    void submit();
    void present(const TaskPresentDesc &present_desc);

    // Execution operations
    void execute(const TaskExecuteDesc &execute_desc);

    void insert_barrier(CommandBatcher &batcher, const TaskBarrier &barrier);
    void set_scope_descriptors(u32 frame_id, TaskSubmitScope &scope, CommandList &cmd_list);

    eastl::tuple<BufferID, void *> allocate_transient_buffer(const BufferDesc &buf_desc);
    auto &get_image_info(TaskImageID image_id) { return m_image_infos[get_handle_val(image_id)]; }
    auto &get_buffer_info(TaskBufferID buffer_id) { return m_buffer_infos[get_handle_val(buffer_id)]; }

    Device *m_device = nullptr;
    SwapChain *m_swap_chain = nullptr;
    PipelineManager m_pipeline_manager = {};
    DescriptorManager m_descriptor_manager = {};
    MemoryAllocator m_memory_allocator = {};

    eastl::vector<TaskFrame> m_frames = {};

    eastl::vector<TaskImageInfo> m_image_infos = {};
    eastl::vector<TaskBufferInfo> m_buffer_infos = {};

    eastl::vector<TaskBarrier> m_barriers = {};
    eastl::vector<TaskBatch> m_batches = {};
    eastl::vector<TaskSubmitScope> m_submit_scopes = {};
    eastl::vector<CommandList> m_command_lists = {};

    eastl::vector<Task *> m_tasks = {};
    Memory::AreaAllocator m_task_allocator = {};

    DescriptorSetID m_sampler_descriptor_id = DescriptorSetID::Invalid;
    DescriptorSetID m_image_descriptor_id = DescriptorSetID::Invalid;
    DescriptorSetID m_rw_image_descriptor_id = DescriptorSetID::Invalid;
    DescriptorSetID m_buffer_address_descriptor_id = DescriptorSetID::Invalid;
};

template<typename TaskT>
void TaskGraph::add_task(const TaskT &task_info)
{
    ZoneScoped;

    auto wrapper = m_task_allocator.allocate_obj<TaskWrapper<TaskT>>(task_info);
    auto task = static_cast<Task *>(wrapper);
    TaskID id = m_tasks.size();
    m_tasks.push_back(task);

    add_task(*task, id);
}

}  // namespace lr::Graphics
