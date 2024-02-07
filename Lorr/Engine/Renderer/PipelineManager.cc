#include "PipelineManager.hh"

#include "Core/FileSystem.hh"

namespace lr::Graphics {
void PipelineCompileInfo::add_shader(eastl::string_view path)
{
    ZoneScoped;

    ShaderCompileDesc compile_desc = {
        .m_code = fs::read_file(path),
#if _DEBUG
        .m_flags = ShaderCompileFlag::SkipOptimization | ShaderCompileFlag::GenerateDebugInfo,
#endif
    };
    m_compile_descs.push_back(compile_desc);
}

PipelineCompileInfo::PipelineCompileInfo(usize color_attachment_count)
{
    ZoneScoped;

    m_graphics_pipeline_info.m_blend_attachments.resize(color_attachment_count);
}

void PipelineCompileInfo::add_include_dir(eastl::string_view path)
{
    ZoneScoped;

    m_include_dirs.push_back(eastl::string(path));
}

void PipelineCompileInfo::add_definition(eastl::string_view definition, eastl::string_view equal_to)
{
    ZoneScoped;

    if (equal_to.empty())
        m_definitions.push_back(eastl::string(definition));
    else
        m_definitions.push_back(eastl::format("{}={}", definition, equal_to));
}

void PipelineCompileInfo::set_dynamic_state(DynamicState dynamic_state)
{
    ZoneScoped;

#define ADD_DYNAMIC_STATE(state)             \
    if (dynamic_state & DynamicState::state) \
    m_graphics_pipeline_info.m_dynamic_states.push_back(DynamicState::VK_##state)

    ADD_DYNAMIC_STATE(Viewport);
    ADD_DYNAMIC_STATE(Scissor);
    ADD_DYNAMIC_STATE(ViewportAndCount);
    ADD_DYNAMIC_STATE(ScissorAndCount);
    ADD_DYNAMIC_STATE(DepthTestEnable);
    ADD_DYNAMIC_STATE(DepthWriteEnable);
    ADD_DYNAMIC_STATE(LineWidth);
    ADD_DYNAMIC_STATE(DepthBias);
    ADD_DYNAMIC_STATE(BlendConstants);
}

void PipelineCompileInfo::set_viewport(u32 id, const glm::vec4 &viewport, const glm::vec2 &depth)
{
    ZoneScoped;

    auto &viewports = m_graphics_pipeline_info.m_viewports;
    if (id + 1 > viewports.size())
        viewports.resize(id + 1);

    viewports[id] = {
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.z,
        .height = viewport.w,
        .minDepth = depth.x,
        .maxDepth = depth.y,
    };
}

void PipelineCompileInfo::set_scissors(u32 id, const glm::uvec4 &rect)
{
    ZoneScoped;

    auto &scissors = m_graphics_pipeline_info.m_scissors;
    if (id + 1 > scissors.size())
        scissors.resize(id + 1);

    scissors[id] = {
        .offset.x = static_cast<i32>(rect.x),
        .offset.y = static_cast<i32>(rect.y),
        .extent.width = rect.z,
        .extent.height = rect.w,
    };
}

void PipelineCompileInfo::set_blend_state(u32 id, const ColorBlendAttachment &state)
{
    ZoneScoped;

    auto &blend_attachments = m_graphics_pipeline_info.m_blend_attachments;
    if (id + 1 > blend_attachments.size())
        blend_attachments.resize(id + 1);

    blend_attachments[id] = state;
}

void PipelineCompileInfo::set_blend_state_all(const ColorBlendAttachment &state)
{
    ZoneScoped;

    auto &blend_attachments = m_graphics_pipeline_info.m_blend_attachments;
    assert(!blend_attachments.empty());
    for (auto &attachment : blend_attachments)
        attachment = state;
}

void PipelineCompileInfo::set_input_layout(const eastl::vector<Format> &formats)
{
    ZoneScoped;

    m_graphics_pipeline_info.m_vertex_attrib_infos.clear();
    m_graphics_pipeline_info.m_vertex_binding_infos.clear();

    u32 offset = 0;
    for (u32 i = 0; i < formats.size(); i++) {
        m_graphics_pipeline_info.m_vertex_attrib_infos.push_back({ 0, i, formats[i], offset });
        offset += format_to_size(formats[i]);
    }
    m_graphics_pipeline_info.m_vertex_binding_infos.push_back({ 0, offset, false });
}

void PipelineCompileInfo::set_descriptor_count(u32 count)
{
    ZoneScoped;

    m_descriptor_count = count;
}

void PipelineManager::init(Device *device)
{
    m_device = device;
}

PipelineManager::~PipelineManager()
{
    ZoneScoped;

    for (auto &shader : m_shaders.m_resources)
        m_device->delete_shader(&shader);
    for (auto &pipeline : m_pipelines.m_resources)
        m_device->delete_pipeline(&pipeline);
}

PipelineInfoID PipelineManager::compile_pipeline(PipelineCompileInfo &compile_info, PipelineAttachmentInfo &attachment_info, PipelineLayout *layout)
{
    ZoneScoped;

    auto [pipeline_info_id, pipeline_info] = m_pipeline_infos.add_resource();
    if (!is_handle_valid(pipeline_info_id)) {
        LOG_ERROR("Cannot compile pipeline info! Resource pool is full.");
        return PipelineInfoID::Invalid;
    }

    auto &graphics_pipeline_info = compile_info.m_graphics_pipeline_info;
    for (auto &compile_desc : compile_info.m_compile_descs) {
        compile_desc.m_include_dirs = compile_info.m_include_dirs;
        auto ir = ShaderCompiler::compile_shader(&compile_desc);

        ShaderReflectionDesc reflection_desc = {};
        auto reflection_data = ShaderCompiler::reflect_spirv(&reflection_desc, ir);
        auto [shader_id, shader] = m_shaders.add_resource();
        if (!is_handle_valid(shader_id)) {
            LOG_ERROR("Cannot create shader! Resource pool is full.");
            return PipelineInfoID::Invalid;
        }

        m_device->create_shader(shader, reflection_data.m_compiled_stage, ir);
        graphics_pipeline_info.m_shader_stages.push_back(PipelineShaderStageInfo(shader, reflection_data.m_entry_point));

        pipeline_info->m_bound_shaders.push_back(shader_id);
        pipeline_info->m_reflections.push_back(reflection_data);
    }

    graphics_pipeline_info.m_layout = layout;

    auto [pipeline_id, pipeline] = m_pipelines.add_resource();
    if (!is_handle_valid(pipeline_id)) {
        LOG_ERROR("Cannot create pipelines. Resource pool is full.");
        return PipelineInfoID::Invalid;
    }

    pipeline_info->m_pipeline_id = pipeline_id;
    m_device->create_graphics_pipeline(pipeline, &graphics_pipeline_info, &attachment_info);

    return pipeline_info_id;
}

PipelineManager::PipelineInfo *PipelineManager::get_pipeline_info(PipelineInfoID pipeline_id)
{
    ZoneScoped;

    return &m_pipeline_infos.get_resource(pipeline_id);
}

Pipeline *PipelineManager::get_pipeline(PipelineID pipeline_id)
{
    ZoneScoped;

    return &m_pipelines.get_resource(pipeline_id);
}

}  // namespace lr::Graphics
