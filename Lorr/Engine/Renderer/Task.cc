#include "Task.hh"

#include "TaskGraph.hh"

namespace lr::Graphics
{
CommandList &TaskContext::get_command_list()
{
    ZoneScoped;

    return m_command_list;
}

ImageView *TaskContext::get_image_view(GenericResource &use)
{
    ZoneScoped;

    return m_task_graph.m_image_infos[use.m_image_id].m_view;
}

glm::uvec2 TaskContext::get_image_size(GenericResource &use)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_task_graph.m_image_infos[use.m_image_id];
    return { imageInfo.m_image->m_width, imageInfo.m_image->m_height };
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

TaskContext::attachment_pair TaskContext::get_attachment_formats()
{
    ZoneScoped;

    attachment_pair formats = {};
    for (auto &resource : m_task.m_generic_resources)
    {
        if (resource.m_image_id == LR_NULL_ID)
            continue;

        TaskImageInfo &imageInfo = m_task_graph.m_image_infos[resource.m_image_id];
        if (resource.m_image_layout == ImageLayout::ColorAttachment)
            formats.first.push_back(imageInfo.m_view->m_format);
        else if (resource.m_image_layout == ImageLayout::DepthStencilAttachment)
            formats.second = imageInfo.m_view->m_format;
    }

    return formats;
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
}  // namespace lr::Renderer
