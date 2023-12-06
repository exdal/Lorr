#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Pipeline.hh"
#include "Graphics/Shader.hh"

#include <EASTL/unordered_map.h>

namespace lr::Renderer
{
struct PipelineCompileInfo
{
    void add_shader(eastl::string_view path, eastl::string_view working_dir = {});
    void add_definition(eastl::string_view definition, eastl::string_view equal_to = {});

    eastl::vector<Graphics::ShaderCompileDesc> m_compile_descs = {};
    eastl::vector<eastl::string> m_definitions = {};
};

struct PipelineManager
{
    void init(Graphics::Device *device);

    struct PipelineInfo
    {
        eastl::vector<Graphics::ShaderReflectionData> m_reflections = {};
        eastl::vector<Graphics::Shader *> m_bound_shaders = {};
        Graphics::Pipeline *m_pipeline = nullptr;
        u32 m_pipeline_id = ~0;
    };

    void create_pipeline(eastl::string_view name, PipelineCompileInfo &compile_info);
    void compile_pipeline(eastl::string_view name, Graphics::PipelineAttachmentInfo &attachment_info);

    PipelineInfo *get_pipeline_info(eastl::string_view name);
    Graphics::Pipeline *get_graphics_pipeline(eastl::string_view name);
    Graphics::GraphicsPipelineInfo *get_graphics_pipeline_info(eastl::string_view name);

    eastl::vector<Graphics::GraphicsPipelineInfo> m_graphics_pipeline_infos = {};
    eastl::unordered_map<eastl::string_view, PipelineInfo> m_pipelines = {};
    Graphics::PipelineLayout m_bindless_layout = nullptr;
    Graphics::Device *m_device = nullptr;
};

}  // namespace lr::Renderer