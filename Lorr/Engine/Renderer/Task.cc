#include "Task.hh"

#include "TaskGraph.hh"

namespace lr::Renderer
{
Graphics::ImageView *TaskContext::get_image_view(GenericResource &use)
{
    ZoneScoped;

    return m_task_graph.m_image_infos[use.m_image_id].m_view;
}

Graphics::RenderingAttachment TaskContext::as_color_attachment(
    GenericResource &use, const Graphics::ColorClearValue &clearVal)
{
    ZoneScoped;

    using namespace Graphics;

    bool is_read = use.m_access.m_access & MemoryAccess::Read;
    bool is_write = use.m_access.m_access & MemoryAccess::Write;
    AttachmentOp load_op = is_read ? AttachmentOp::Load : AttachmentOp::Clear;
    AttachmentOp store_op = is_write ? AttachmentOp::Store : AttachmentOp::DontCare;
    return RenderingAttachment(
        get_image_view(use), use.m_image_layout, load_op, store_op, clearVal);
}

glm::uvec2 TaskContext::get_image_size(GenericResource &use)
{
    ZoneScoped;

    TaskImageInfo &imageInfo = m_task_graph.m_image_infos[use.m_image_id];
    return { imageInfo.m_image->m_width, imageInfo.m_image->m_height };
}
}  // namespace lr::Renderer
