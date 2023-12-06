#pragma once

#include "Task.hh"
#include "TaskCommandList.hh"
#include "TaskResource.hh"

namespace lr::Renderer
{
struct TaskGraph;
struct TaskContext
{
    TaskContext(Task *task, TaskGraph &task_graph, TaskCommandList &command_list)
        : m_task(task),
          m_task_graph(task_graph),
          m_command_list(command_list)
    {
    }

    TaskCommandList &get_command_list();
    Graphics::ImageView *get_image_view(GenericResource &use);
    glm::uvec2 get_image_size(GenericResource &use);
    glm::uvec4 get_pass_size();
    using attachment_pair = eastl::pair<eastl::vector<Graphics::Format>, Graphics::Format>;
    attachment_pair get_attachment_formats();
    eastl::span<GenericResource> get_resources();

    Graphics::RenderingAttachment as_color_attachment(
        GenericResource &use, const Graphics::ColorClearValue &clear_value = { 0.0f, 0.0f, 0.0f, 1.0f });

private:
    Task *m_task;
    TaskGraph &m_task_graph;
    TaskCommandList &m_command_list;
};

using TaskID = u32;
struct Task
{
    virtual ~Task() = default;
    virtual void execute(TaskContext &ctx) = 0;

    template<typename ImageFn, typename BufferFn>
    void for_each_res(const ImageFn &image_cb, const BufferFn &buffer_cb)
    {
        for (GenericResource &resource : m_generic_resources)
        {
            switch (resource.m_type)
            {
                case TaskResourceType::Image:
                    image_cb(resource);
                    break;
                case TaskResourceType::Buffer:
                    buffer_cb(resource);
                    break;
            }
        }
    }

    eastl::string_view m_name;
    eastl::span<GenericResource> m_generic_resources;
};

template<typename TTask>
concept TaskConcept = requires { TTask{}.m_uses; } and requires(TaskContext tc) { TTask{}.execute(tc); };

template<TaskConcept TTask>
struct TaskWrapper : Task
{
    TaskWrapper(const TTask &task)
        : m_task(task)
    {
        m_name = TTask::m_task_name;
        m_generic_resources = { (GenericResource *)&m_task.m_uses, resource_count };
#if _DEBUG
        for (auto &resource : m_generic_resources)
            assert(resource.m_buffer_id != BufferNull);
#endif
    }

    void execute(TaskContext &ctx) override { m_task.execute(ctx); }

    TTask m_task;
    constexpr static usize resource_count = sizeof(typename TTask::Uses) / sizeof(GenericResource);
};

struct TaskBarrier
{
    ImageID m_image_id = ImageNull;
    Graphics::ImageLayout m_src_layout = {};
    Graphics::ImageLayout m_dst_layout = {};
    TaskAccess::Access m_src_access = {};
    TaskAccess::Access m_dst_access = {};
};

using TaskBatchID = u32;
constexpr static TaskBatchID kTaskBatchNull = ~0;
struct TaskBatch
{
    TaskAccess::Access m_execution_access = TaskAccess::None;
    eastl::vector<usize> m_wait_barriers = {};
    eastl::vector<TaskID> m_tasks = {};

    // For resources are not connected to any other passes but still need a barrier
    // For example, SwapChain image needs to be in present layout at the end of execution
    eastl::vector<usize> m_end_barriers = {};
};

}  // namespace lr::Renderer
