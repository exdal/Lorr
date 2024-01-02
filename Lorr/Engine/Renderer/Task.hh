#pragma once

#include "PipelineManager.hh"
#include "Task.hh"
#include "TaskResource.hh"

namespace lr::Graphics
{
struct Task;
struct TaskGraph;
struct TaskContext
{
    TaskContext(Task &task, TaskGraph &task_graph, CommandList &command_list)
        : m_task(task),
          m_task_graph(task_graph),
          m_command_list(command_list){};

    CommandList &get_command_list();
    Pipeline *get_pipeline();
    ImageView *get_image_view(GenericResource &use);
    glm::uvec2 get_image_size(GenericResource &use);
    glm::uvec4 get_pass_size();
    eastl::tuple<eastl::vector<Format>, Format> get_attachment_formats();
    eastl::span<GenericResource> get_resources();

    template<typename T>
    void set_push_constant(const T &v, u32 offset = 0)
    {
        offset += 4;
        m_command_list.set_push_constants(&v, sizeof(T), offset, get_pipeline()->m_layout);
    }

    RenderingAttachment as_color_attachment(GenericResource &use, const ColorClearValue &clear_value = { 0.0f, 0.0f, 0.0f, 1.0f });

private:
    Task &m_task;
    TaskGraph &m_task_graph;
    CommandList &m_command_list;
};

using TaskID = u32;
struct Task
{
    virtual ~Task() = default;
    virtual void execute(TaskContext &ctx) = 0;
    virtual bool compile_pipeline(PipelineCompileInfo &compile_info) = 0;

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
    PipelineID m_pipeline_id = {};
};

template<typename TaskT>
concept TaskConcept = requires { TaskT{}.m_uses; } and requires(TaskContext &tc) { TaskT{}.execute(tc); };

template<TaskConcept TaskT>
struct TaskWrapper : Task
{
    TaskWrapper(const TaskT &task)
        : m_task(task)
    {
        m_name = TaskT::m_task_name;
        m_generic_resources = { (GenericResource *)&m_task.m_uses, resource_count };
#if _DEBUG
        for (auto &resource : m_generic_resources)
            assert(is_handle_valid(resource.m_task_buffer_id));
#endif
    }

    void execute(TaskContext &ctx) override { m_task.execute(ctx); }
    bool compile_pipeline(PipelineCompileInfo &compile_info) override
    {
        constexpr bool has_pipeline = requires(PipelineCompileInfo &pci) { TaskT{}.compile_pipeline(pci); };
        if constexpr (has_pipeline)
            m_task.compile_pipeline(compile_info);

        return has_pipeline;
    }

    TaskT m_task;
    constexpr static usize resource_count = sizeof(typename TaskT::Uses) / sizeof(GenericResource);
};

struct TaskBarrier
{
    ImageID m_image_id = {};
    ImageLayout m_src_layout = {};
    ImageLayout m_dst_layout = {};
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

struct TaskSubmitScope
{
    eastl::vector<Semaphore *> m_wait_semas = {};
    eastl::vector<TaskBatchID> m_batches = {};
    eastl::vector<Semaphore *> m_signal_semas = {};
};

}  // namespace lr::Graphics
