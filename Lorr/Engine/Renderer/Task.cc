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

    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    auto pipeline_info = pipeline_man.get_pipeline_info(m_task.m_pipeline_info_id);
    return pipeline_man.get_pipeline(pipeline_info->m_pipeline_id);
}

ShaderStructRange *TaskContext::get_descriptor_range(ShaderStage shader_stage)
{
    ZoneScoped;

    auto &pipeline_man = m_task_graph.m_pipeline_manager;
    auto pipeline_info = pipeline_man.get_pipeline_info(m_task.m_pipeline_info_id);
    ShaderStructRange *range = nullptr;

    for (ShaderReflectionData &reflection_data : pipeline_info->m_reflections)
    {
        if (reflection_data.m_compiled_stage != shader_stage)
            continue;

        range = &reflection_data.m_descriptors_struct;
    }

    return range;
}

void TaskContext::set_descriptors(ShaderStage shader_stage, std::initializer_list<GenericResource> resources)
{
    ZoneScoped;

    ShaderStructRange *shader_descriptor_range = get_descriptor_range(shader_stage);
    assert(shader_descriptor_range != nullptr && shader_descriptor_range->size == sizeof(ShaderDescriptors));

    ShaderDescriptors descriptors = {};
    set_push_constant(shader_stage, descriptors);
}

glm::uvec2 TaskContext::get_image_size(GenericResource &resource)
{
    TaskImageInfo &image_info = m_task_graph.get_image_info(resource.m_task_image_id);
    Image *image = m_task_graph.m_device->get_image(image_info.m_image_id);

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
        auto &image_info = m_task_graph.m_image_infos[get_handle_val(resource.m_task_image_id)];
        ImageView *image_view = m_task_graph.m_device->get_image_view(image_info.m_image_view_id);
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
    TaskImageInfo &image_info = m_task_graph.get_image_info(use.m_task_image_id);
    return { m_task_graph.m_device->get_image_view(image_info.m_image_view_id), use.m_image_layout, load_op, store_op, clear_value };
}
}  // namespace lr::Graphics
