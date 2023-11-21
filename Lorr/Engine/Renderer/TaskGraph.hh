#pragma once

#include "Task.hh"
#include "TaskCommandList.hh"
#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"
#include "Memory/Allocator/LinearAllocator.hh"

#include <EASTL/vector.h>

namespace lr::Renderer
{
struct TaskGraphExecuteDesc
{
    u32 m_FrameIndex = 0;
    Graphics::Semaphore *m_pAcquireSema = nullptr;
    Graphics::Semaphore *m_pPresentSema = nullptr;
};

struct TaskGraphDesc
{
    u32 m_frame_count = 0;
    usize m_initial_alloc = 0xffff;
    Graphics::PhysicalDevice *m_physical_device = nullptr;
    Graphics::Device *m_device = nullptr;
};

struct TaskGraph
{
    void create(TaskGraphDesc *desc);

    ImageID use_persistent_image(const PersistentImageInfo &image_info);
    void set_image(ImageID image_id, Graphics::Image *image, Graphics::ImageView *view);
    BufferID use_persistent_buffer(const PersistentBufferInfo &buffer_info);

    TaskBatchID schedule_task(Task &task);
    template<typename TTask>
    void add_task(const TTask &task_info);
    void add_task(Task &task, TaskID id);
    void present_task(ImageID back_buffer_id);

    void execute(const TaskGraphExecuteDesc &desc);

    void insert_barrier(TaskCommandList &cmd_list, const TaskBarrier &barrier);

    Graphics::Device *m_device = nullptr;
    Graphics::CommandQueue *m_graphics_queue = nullptr;
    Graphics::DeviceMemory *m_image_memory = nullptr;

    // TODO(Batching): These gotta go...
    eastl::vector<Graphics::CommandAllocator *> m_command_allocators = {};
    eastl::vector<Graphics::CommandList *> m_command_lists = {};

    eastl::vector<Graphics::Semaphore *> m_semaphores = {};
    eastl::vector<TaskImageInfo> m_image_infos = {};
    eastl::vector<TaskBufferInfo> m_buffer_infos = {};
    eastl::vector<TaskBarrier> m_barriers = {};
    eastl::vector<TaskBatch> m_batches = {};
    eastl::vector<Task *> m_tasks = {};
    Memory::LinearAllocator m_task_allocator = {};
};

template<typename TTask>
void TaskGraph::add_task(const TTask &task_info)
{
    ZoneScoped;

    auto wrapper = m_task_allocator.New<TaskWrapper<TTask>>(task_info);
    Task *pTask = static_cast<Task *>(wrapper);
    TaskID id = m_tasks.size();
    m_tasks.push_back(pTask);

    add_task(*pTask, id);
}

}  // namespace lr::Renderer