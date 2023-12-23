#pragma once

#include "PipelineManager.hh"
#include "Task.hh"
#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"
#include "Memory/Allocator/AreaAllocator.hh"

#include <EASTL/vector.h>

namespace lr::Graphics
{
struct TaskFrame
{
    constexpr static usize k_type_count = static_cast<usize>(CommandType::Count);

    Semaphore m_timeline_sema = {};
    fixed_vector<CommandAllocator, k_type_count> m_allocators = {};
    fixed_vector<CommandList, k_type_count> m_lists = {};
};

struct TaskGraphDesc
{
    Device *m_device = nullptr;
    SwapChain *m_swap_chain = nullptr;
};

struct TaskGraph
{
    void init(TaskGraphDesc *desc);

    ImageID use_persistent_image(const PersistentImageInfo &image_info);
    void set_image(ImageID image_id, Image *image, ImageView *view);
    BufferID use_persistent_buffer(const PersistentBufferInfo &buffer_info);

    TaskBatchID schedule_task(Task &task);
    template<typename TaskT>
    void add_task(const TaskT &task_info);
    void add_task(Task &task, TaskID id);

    void execute(SwapChain *swap_chain);

    void insert_present_barrier(CommandBatcher &batcher, ImageID image_id);
    void insert_barrier(CommandBatcher &batcher, const TaskBarrier &barrier);

    Device *m_device = nullptr;
    CommandQueue m_graphics_queue = {};
    DeviceMemory m_image_memory = {};
    PipelineManager m_pipeline_manager = {};

    eastl::vector<TaskFrame> m_frames = {};

    eastl::vector<TaskImageInfo> m_image_infos = {};
    eastl::vector<TaskBufferInfo> m_buffer_infos = {};
    eastl::vector<TaskBarrier> m_barriers = {};
    eastl::vector<TaskBatch> m_batches = {};
    eastl::vector<Task *> m_tasks = {};
    Memory::AreaAllocator m_task_allocator = {};
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