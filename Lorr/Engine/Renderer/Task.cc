#include "Task.hh"

#include "TaskGraph.hh"

namespace lr::Graphics
{
CommandList &TaskContext::get_command_list()
{
    ZoneScoped;

    return m_command_list;
}

Pipeline *TaskContext::get_pipeline()
{
    ZoneScoped;

    return m_task_graph.m_pipeline_manager.get_pipeline(m_task.m_pipeline_id);
}

ImageView *TaskContext::get_image_view(GenericResource &use)
{
    ZoneScoped;

    return m_task_graph.get_image_view(use.m_task_image_id);
}

glm::uvec2 TaskContext::get_image_size(GenericResource &use)
{
    ZoneScoped;

    Image *image = m_task_graph.get_image(use.m_task_image_id);
    return { image->m_width, image->m_height };
}

glm::uvec4 TaskContext::get_pass_size()
{
    ZoneScoped;

    glm::uvec2 max_size = {};
    for (auto &use : m_task.m_generic_resources)
    {
        auto size = get_image_size(use);
        if (max_size.x < size.x && max_size.y < size.y)
            max_size = size;
    }

    return { 0, 0, max_size.x, max_size.y };
}

eastl::tuple<eastl::vector<Format>, Format> TaskContext::get_attachment_formats()
{
    ZoneScoped;

    eastl::vector<Format> color_formats = {};
    Format depth_format = {};
    for (auto &resource : m_task.m_generic_resources)
    {
        if (!is_handle_valid(resource.m_task_image_id))
            continue;

        ImageView *image_view = m_task_graph.get_image_view(resource.m_task_image_id);
        if (resource.m_image_layout == ImageLayout::ColorAttachment)
            color_formats.push_back(image_view->m_format);
        else if (resource.m_image_layout == ImageLayout::DepthStencilAttachment)
            depth_format = image_view->m_format;
    }

    return { eastl::move(color_formats), depth_format };
}

eastl::span<GenericResource> TaskContext::get_resources()
{
    return m_task.m_generic_resources;
}

RenderingAttachment TaskContext::as_color_attachment(GenericResource &use, const ColorClearValue &clear_value)
{
    ZoneScoped;

    using namespace Graphics;
    bool is_read = use.m_access.m_access & MemoryAccess::Read;
    bool is_write = use.m_access.m_access & MemoryAccess::Write;
    AttachmentOp load_op = is_read ? AttachmentOp::Load : AttachmentOp::Clear;
    AttachmentOp store_op = is_write ? AttachmentOp::Store : AttachmentOp::DontCare;
    return RenderingAttachment(get_image_view(use), use.m_image_layout, load_op, store_op, clear_value);
}
}  // namespace lr::Graphics
