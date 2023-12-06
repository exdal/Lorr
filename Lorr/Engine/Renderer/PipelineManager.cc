#include "PipelineManager.hh"

#include "Core/FileSystem.hh"

namespace lr::Renderer
{
using namespace Graphics;

void PipelineCompileInfo::add_shader(eastl::string_view path, eastl::string_view working_dir)
{
    ZoneScoped;

    auto current_dir = fs::get_current_dir();
    if (working_dir.empty())
        working_dir = current_dir;

    ShaderCompileDesc compile_desc = {
        .m_code = fs::read_file(path),
#if _DEBUG
        .m_flags = ShaderCompileFlag::SkipOptimization | ShaderCompileFlag::GenerateDebugInfo,
#endif
    };
    compile_desc.m_working_dir = working_dir;
    m_compile_descs.push_back(compile_desc);
}

void PipelineCompileInfo::add_definition(eastl::string_view definition, eastl::string_view equal_to)
{
    ZoneScoped;

    if (equal_to.empty())
        m_definitions.push_back(eastl::string(definition));
    else
        m_definitions.push_back(eastl::format("{}={}", definition, equal_to));
}

void PipelineManager::init(Graphics::Device *device)
{
    m_device = device;

    m_bindless_layout = m_device->create_pipeline_layout({}, {});
}

void PipelineManager::create_pipeline(eastl::string_view name, PipelineCompileInfo &compile_info)
{
    ZoneScoped;

    PipelineInfo pipeline_info = {};

    for (auto &compile_desc : compile_info.m_compile_descs)
    {
        ShaderReflectionDesc reflection_desc = {
            .m_reflect_vertex_layout = true,
        };

        auto ir = ShaderCompiler::compile_shader(&compile_desc);
        auto reflection_data = ShaderCompiler::reflect_spirv(&reflection_desc, ir);

        auto shader = m_device->create_shader(reflection_data.m_compiled_stage, ir);
        pipeline_info.m_reflections.push_back(reflection_data);
        pipeline_info.m_bound_shaders.push_back(shader);
    }

    pipeline_info.m_pipeline_id = m_graphics_pipeline_infos.size();
    m_graphics_pipeline_infos.push_back({});

    m_pipelines.emplace(name, pipeline_info);
}

void PipelineManager::compile_pipeline(eastl::string_view name, PipelineAttachmentInfo &attachment_info)
{
    ZoneScoped;

    auto pipeline_info = get_pipeline_info(name);
    if (!pipeline_info)
    {
        LOG_ERROR("Trying to build null pipeline<{}>.", name);
        return;
    }

    auto &pipeline = pipeline_info->m_pipeline;
    assert(pipeline == nullptr);

    auto &compile_info = m_graphics_pipeline_infos[pipeline_info->m_pipeline_id];
    for (u32 i = 0; i < pipeline_info->m_bound_shaders.size(); i++)
    {
        auto &reflection = pipeline_info->m_reflections[i];
        auto &shader = pipeline_info->m_bound_shaders[i];
        compile_info.m_vertex_attrib_infos.insert(
            compile_info.m_vertex_attrib_infos.end(), reflection.m_vertex_attribs.begin(), reflection.m_vertex_attribs.end());

        compile_info.m_shader_stages.push_back(PipelineShaderStageInfo(shader, reflection.m_entry_point));
    }

    if (!compile_info.m_vertex_attrib_infos.empty())
        compile_info.m_vertex_binding_infos.push_back(PipelineVertexLayoutBindingInfo(0, 0, false));

    compile_info.m_layout = m_bindless_layout;

    pipeline = m_device->create_graphics_pipeline(&compile_info, &attachment_info);
}

PipelineManager::PipelineInfo *PipelineManager::get_pipeline_info(eastl::string_view name)
{
    ZoneScoped;

    auto iterator = m_pipelines.find(name);
    if (iterator == m_pipelines.end())
        return nullptr;

    return &iterator->second;
}

Pipeline *PipelineManager::get_graphics_pipeline(eastl::string_view name)
{
    ZoneScoped;

    auto info = get_pipeline_info(name);
    if (info == nullptr)
        return nullptr;

    return info->m_pipeline;
}

GraphicsPipelineInfo *PipelineManager::get_graphics_pipeline_info(eastl::string_view name)
{
    ZoneScoped;

    auto info = get_pipeline_info(name);
    if (info == nullptr)
        return nullptr;

    return &m_graphics_pipeline_infos[info->m_pipeline_id];
}
}  // namespace lr::Renderer