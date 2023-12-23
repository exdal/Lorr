#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Pipeline.hh"
#include "Graphics/Shader.hh"

#include <EASTL/unordered_map.h>

namespace lr::Graphics
{
struct PipelineCompileInfo
{
    /// SHADERS ///
    void add_include_dir(eastl::string_view path);
    void add_shader(eastl::string_view path);
    void add_definition(eastl::string_view definition, eastl::string_view equal_to = {});

    /// PIPELINE ///
    void set_dynamic_state(DynamicState dynamic_state);
    void set_viewport(u32 id, const glm::vec4 &viewport, const glm::vec2 &depth = { 1.0, 0.01 });
    void set_scissors(u32 id, const glm::uvec4 &rect);
    void set_blend_state(u32 id, const ColorBlendAttachment &state);
    void set_blend_state_all(const ColorBlendAttachment &state);

    GraphicsPipelineInfo m_pipeline_info = {};
    eastl::vector<ShaderCompileDesc> m_compile_descs = {};
    eastl::vector<eastl::string> m_definitions = {};
    eastl::vector<eastl::string> m_include_dirs = {};
};

struct PipelineManager
{
    void init(Device *device);

    struct PipelineInfo
    {
        eastl::vector<ShaderReflectionData> m_reflections = {};
        eastl::vector<usize> m_bound_shaders = {};
        u32 m_pipeline_id = LR_NULL_ID;
    };

    void create_pipeline(eastl::string_view name, PipelineCompileInfo &compile_info);
    void compile_pipeline(eastl::string_view name, PipelineAttachmentInfo &attachment_info);

    PipelineInfo *get_pipeline_info(eastl::string_view name);
    Pipeline *get_pipeline(eastl::string_view name);
    GraphicsPipelineInfo *get_graphics_pipeline_info(eastl::string_view name);

    eastl::vector<Shader> m_shaders = {};
    eastl::vector<Pipeline> m_pipelines = {};
    eastl::vector<GraphicsPipelineInfo> m_graphics_pipeline_infos = {};
    eastl::unordered_map<eastl::string_view, PipelineInfo> m_pipeline_infos = {};

    PipelineLayout m_bindless_layout = nullptr;
    Device *m_device = nullptr;
};

}  // namespace lr::Graphics