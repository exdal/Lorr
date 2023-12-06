#include "TaskCommandList.hh"

#include "Task.hh"
#include "TaskGraph.hh"

namespace lr::Renderer
{
using namespace Graphics;
TaskCommandList::TaskCommandList(VkCommandBuffer command_buffer, TaskGraph &task_graph)
    : m_handle(command_buffer),
      m_task_graph(task_graph)
{
}

TaskCommandList::~TaskCommandList()
{
    flush_barriers();
}

void TaskCommandList::set_context(TaskContext *context)
{
    m_context = context;
    m_bound_pipeline_name.clear();
    m_pipeline = nullptr;
}

void TaskCommandList::insert_memory_barrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_memory_barriers.full())
        flush_barriers();

    m_memory_barriers.push_back(barrier);
}

void TaskCommandList::insert_image_barrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_image_barriers.full())
        flush_barriers();

    m_image_barriers.push_back(barrier);
}

void TaskCommandList::flush_barriers()
{
    ZoneScoped;

    if (m_image_barriers.empty() && m_memory_barriers.empty())
        return;

    DependencyInfo dep_info(m_image_barriers, m_memory_barriers);
    vkCmdPipelineBarrier2(m_handle, &dep_info);

    m_image_barriers.clear();
    m_memory_barriers.clear();
}

void TaskCommandList::begin_rendering(const RenderingBeginDesc &begin_desc)
{
    ZoneScoped;

    glm::uvec4 render_area = begin_desc.m_render_area;
    if (render_area == render_area_whole)
        render_area = m_context->get_pass_size();

    VkRenderingInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .layerCount = 1,
        .viewMask = 0,  // TODO: Multiviews
        .colorAttachmentCount = static_cast<u32>(begin_desc.m_color_attachments.size()),
        .pColorAttachments = begin_desc.m_color_attachments.data(),
        .pDepthAttachment = begin_desc.m_depth_attachment,
    };
    beginInfo.renderArea.offset.x = static_cast<i32>(render_area.x);
    beginInfo.renderArea.offset.y = static_cast<i32>(render_area.y);
    beginInfo.renderArea.extent.width = render_area.z;
    beginInfo.renderArea.extent.height = render_area.w;

    vkCmdBeginRendering(m_handle, &beginInfo);
}

void TaskCommandList::end_rendering()
{
    ZoneScoped;

    vkCmdEndRendering(m_handle);
}

TaskCommandList &TaskCommandList::set_dynamic_state(DynamicState states)
{
    ZoneScoped;

    if (!m_pipeline)
        m_dynamic_states = states;

    return *this;
}

TaskCommandList &TaskCommandList::set_pipeline(eastl::string_view pipeline_name)
{
    ZoneScoped;

    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    m_bound_pipeline_name = pipeline_name;
    m_pipeline = pipeline_man.get_graphics_pipeline(pipeline_name);

    // Preinit stuff here
    if (!m_pipeline)
    {
        auto [color_formats, _] = m_context->get_attachment_formats();
        auto compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
        compile_info->m_blend_attachments.resize(color_formats.size());
    }

    return *this;
}

TaskCommandList &TaskCommandList::set_viewport(u32 id, const glm::vec4 &viewport, const glm::vec2 &depth)
{
    ZoneScoped;

    const bool is_dynamic = m_dynamic_states & (DynamicState::Viewport | DynamicState::ViewportAndCount);
    VkViewport vp = {
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.z,
        .height = viewport.w,
        .minDepth = depth.x,
        .maxDepth = depth.y,
    };

    if (m_pipeline == nullptr)
    {
        auto &pipeline_man = m_task_graph.m_pipeline_manager;
        auto compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
        if (id + 1 > compile_info->m_viewports.size())
            compile_info->m_viewports.resize(id + 1);

        compile_info->m_viewports[id] = vp;
    }

    if (is_dynamic)
        vkCmdSetViewport(m_handle, id, 1, &vp);

    return *this;
}

TaskCommandList &TaskCommandList::set_scissors(u32 id, const glm::uvec4 &rect)
{
    ZoneScoped;

    const bool is_dynamic = m_dynamic_states & (DynamicState::Scissor | DynamicState::ScissorAndCount);
    VkRect2D scissor = {
        .offset.x = static_cast<i32>(rect.x),
        .offset.y = static_cast<i32>(rect.y),
        .extent.width = rect.z,
        .extent.height = rect.w,
    };

    if (m_pipeline == nullptr)
    {
        auto &pipeline_man = m_task_graph.m_pipeline_manager;
        auto compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
        if (id + 1 > compile_info->m_scissors.size())
            compile_info->m_scissors.resize(id + 1);

        compile_info->m_scissors[id] = scissor;
    }

    if (is_dynamic)
        vkCmdSetScissor(m_handle, id, 1, &scissor);

    return *this;
}

TaskCommandList &TaskCommandList::set_blend_state(u32 id, const ColorBlendAttachment &state)
{
    ZoneScoped;

    if (m_pipeline != nullptr)
        return *this;

    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    auto compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
    auto &attachments = compile_info->m_blend_attachments;

    attachments[id] = state;

    return *this;
}

TaskCommandList &TaskCommandList::set_blend_state_all(const Graphics::ColorBlendAttachment &state)
{
    ZoneScoped;

    if (m_pipeline != nullptr)
        return *this;

    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    auto compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
    auto &attachments = compile_info->m_blend_attachments;

    for (auto &attachment : attachments)
        attachment = state;

    return *this;
}

void TaskCommandList::draw(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance)
{
    ZoneScoped;

    compile_and_bind_pipeline();
    vkCmdDraw(m_handle, vertex_count, instance_count, first_vertex, first_instance);
}

void TaskCommandList::draw_indexed(u32 index_count, u32 first_index, u32 vertex_offset, u32 instance_count, u32 first_instance)
{
    ZoneScoped;

    compile_and_bind_pipeline();
    vkCmdDrawIndexed(m_handle, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void TaskCommandList::dispatch(u32 group_x, u32 group_y, u32 group_z)
{
    ZoneScoped;

    compile_and_bind_pipeline();
    vkCmdDispatch(m_handle, group_x, group_y, group_z);
}

void TaskCommandList::compile_and_bind_pipeline()
{
    ZoneScoped;

    assert(!m_bound_pipeline_name.empty());
    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    auto pipeline_info = pipeline_man.get_pipeline_info(m_bound_pipeline_name);
    if (!pipeline_info)
    {
        LOG_ERROR("Null pipeline cannot be bound to command list.");
        return;
    }

    assert(pipeline_info);
    auto &pipeline = pipeline_info->m_pipeline;
    if (!pipeline)
    {
        // Compute pipeline will be built no matter what
        GraphicsPipelineInfo *compile_info = pipeline_man.get_graphics_pipeline_info(m_bound_pipeline_name);
        eastl::vector<DynamicState> dynamic_states = {};

#define ADD_DYNAMIC_STATE(state)                \
    if (m_dynamic_states & DynamicState::state) \
    dynamic_states.push_back(DynamicState::VK_##state)

        ADD_DYNAMIC_STATE(Viewport);
        ADD_DYNAMIC_STATE(Scissor);
        ADD_DYNAMIC_STATE(ViewportAndCount);
        ADD_DYNAMIC_STATE(ScissorAndCount);
        ADD_DYNAMIC_STATE(DepthTestEnable);
        ADD_DYNAMIC_STATE(DepthWriteEnable);
        ADD_DYNAMIC_STATE(LineWidth);
        ADD_DYNAMIC_STATE(DepthBias);
        ADD_DYNAMIC_STATE(BlendConstants);
        compile_info->m_dynamic_states = dynamic_states;

        auto [color_formats, depth_formats] = m_context->get_attachment_formats();
        PipelineAttachmentInfo attachment_info = PipelineAttachmentInfo(color_formats, depth_formats);
        pipeline_man.compile_pipeline(m_bound_pipeline_name, attachment_info);

        m_pipeline = pipeline;
    }

    vkCmdBindPipeline(m_handle, pipeline->m_bind_point, *m_pipeline);
}

}  // namespace lr::Renderer
