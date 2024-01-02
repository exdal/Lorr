#pragma once

#include "CommandBatcher.hh"
#include "PipelineManager.hh"
#include "Task.hh"
#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"
#include "Graphics/MemoryAllocator.hh"
#include "Memory/Allocator/AreaAllocator.hh"

#include <EASTL/vector.h>
#include <EASTL/queue.h>

namespace lr::Graphics
{
struct TaskFrame
{
    constexpr static usize k_type_count = static_cast<usize>(CommandType::Count);

    Semaphore m_timeline_sema = {};
    eastl::array<CommandAllocator, k_type_count> m_allocators = {};
    eastl::array<CommandList, k_type_count> m_lists = {};
    LinearMemoryPool *m_frametime_memory = nullptr;
    eastl::queue<Buffer> m_zombie_buffers = {};

    DescriptorSet m_descriptor_set = {};
};

struct TaskPresentDesc
{
    ImageID m_swap_chain_image_id = {};
};

struct TaskExecuteDesc
{
    SwapChain *m_swap_chain = nullptr;
    eastl::span<Semaphore *> m_wait_semas = {};
    eastl::span<Semaphore *> m_signal_semas = {};
};

struct TaskGraphDesc
{
    Device *m_device = nullptr;
    u32 m_frame_count = 1;
};

struct TaskGraph
{
    void init(TaskGraphDesc *desc);
    ~TaskGraph();

    eastl::tuple<ImageID, Image *> add_new_image();
    eastl::tuple<ImageID, ImageView *> add_new_image_view();

    Image *get_image(ImageID id);
    ImageView *get_image_view(ImageID id);

    ImageID use_persistent_image(const PersistentImageInfo &persistent_image_info);
    void set_image(ImageID image_info_id, ImageID image_id, ImageID image_view_id);

    BufferID use_persistent_buffer(const PersistentBufferInfo &buffer_info);
    BufferID create_buffer(const BufferDesc &desc);

    TaskBatchID schedule_task(Task &task);
    template<typename TaskT>
    void add_task(const TaskT &task_info);
    void add_task(Task &task, TaskID id);
    void compile_task_pipeline(Task &task);

    void present(const TaskPresentDesc &present_desc);
    void execute(const TaskExecuteDesc &execute_desc);

    void insert_barrier(CommandBatcher &batcher, const TaskBarrier &barrier);

    Device *m_device = nullptr;
    PipelineManager m_pipeline_manager = {};
    DescriptorManager m_descriptor_manager = {};
    MemoryAllocator m_memory_allocator = {};

    // vectors move memories on de/allocation, so using raw pointer is not safe
    // instead we have ResourcePool that is fixed size and does not do allocations
    ResourcePool<Image, ImageID> m_images = {};
    ResourcePool<ImageView, ImageID> m_image_views = {};
    ResourcePool<Buffer, BufferID> m_buffers = {};

    eastl::vector<TaskFrame> m_frames = {};

    eastl::vector<TaskImageInfo> m_image_infos = {};
    eastl::vector<TaskBufferInfo> m_buffer_infos = {};

    eastl::vector<TaskBarrier> m_barriers = {};
    eastl::vector<TaskBatch> m_batches = {};
    eastl::vector<TaskSubmitScope> m_submit_scopes = {};

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