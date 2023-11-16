#pragma once

#include "Task.hh"
#include "TaskResource.hh"

#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"
#include "Memory/Allocator/LinearAllocator.hh"

#include <EASTL/functional.h>
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
    u32 m_FrameCount = 0;
    usize m_InitialAlloc = 0xffff;
    Graphics::PhysicalDevice *m_pPhysDevice = nullptr;
    Graphics::Device *m_pDevice = nullptr;
};

struct TaskGraph
{
    void Init(TaskGraphDesc *pDesc);

    ImageID UsePersistentImage(const PersistentImageInfo &imageInfo);
    void SetImage(ImageID imageID, Graphics::Image *pImage, Graphics::ImageView *pView);

    TaskBatchID ScheduleTask(Task &task);
    template<typename _T>
    void AddTask(const _T &taskInfo);
    void AddTask(Task &task, TaskID id);
    void PresentTask(ImageID backBufferID);

    void Execute(const TaskGraphExecuteDesc &desc);

    Graphics::Device *m_pDevice = nullptr;
    Graphics::CommandQueue *m_pGraphicsQueue = nullptr;
    Graphics::DeviceMemory *m_pImageMemory = nullptr;

    // TODO(Batching): These gotta go...
    eastl::vector<Graphics::CommandAllocator *> m_CommandAllocators = {};
    eastl::vector<Graphics::CommandList *> m_CommandLists = {};

    eastl::vector<Graphics::Semaphore *> m_Semaphores = {};
    eastl::vector<TaskImageInfo> m_ImageInfos = {};
    eastl::vector<TaskBarrier> m_Barriers = {};
    eastl::vector<TaskBatch> m_Batches = {};
    eastl::vector<Task *> m_Tasks = {};
    Memory::LinearAllocator m_TaskAllocator = {};
};

template<typename _T>
void TaskGraph::AddTask(const _T &taskInfo)
{
    ZoneScoped;

    auto wrapper = m_TaskAllocator.New<TaskWrapper<_T>>(taskInfo);
    Task *pTask = static_cast<Task *>(wrapper);
    TaskID id = m_Tasks.size();
    m_Tasks.push_back(pTask);

    AddTask(*pTask, id);
}

}  // namespace lr::Renderer