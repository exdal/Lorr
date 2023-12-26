#pragma once

#include "Graphics/Device.hh"
#include "Graphics/MemoryAllocator.hh"
#include "Graphics/Pipeline.hh"
#include "Graphics/Shader.hh"

namespace lr::Graphics
{
struct PipelineCompileInfo
{
    void init(usize color_attachment_count);

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

    GraphicsPipelineInfo m_graphics_pipeline_info = {};
    eastl::vector<ShaderCompileDesc> m_compile_descs = {};
    eastl::vector<eastl::string> m_definitions = {};
    eastl::vector<eastl::string> m_include_dirs = {};
};

struct PipelineManager
{
    void init(Device *device);
    virtual ~PipelineManager();

    struct PipelineInfo
    {
        eastl::vector<ShaderReflectionData> m_reflections = {};
        eastl::vector<usize> m_bound_shaders = {};
        u32 m_pipeline_id = LR_NULL_ID;
    };

    usize compile_pipeline(PipelineCompileInfo &compile_info, PipelineAttachmentInfo &attachment_info);

    PipelineInfo *get_pipeline_info(usize pipeline_id);
    Pipeline *get_pipeline(usize pipeline_id);

    ResourcePool<Shader> m_shaders = {};
    ResourcePool<Pipeline> m_pipelines = {};
    ResourcePool<PipelineInfo> m_pipeline_infos = {};

    PipelineLayout m_bindless_layout = nullptr;
    Device *m_device = nullptr;
};

}  // namespace lr::Graphics