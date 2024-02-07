#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Pipeline.hh"
#include "Graphics/Shader.hh"

namespace lr::Graphics {
struct PipelineCompileInfo {
    PipelineCompileInfo(usize color_attachment_count);

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
    void set_input_layout(const eastl::vector<Format> &formats);
    void set_descriptor_count(u32 count);
    template<typename T>
    void set_push_constants()
    {
        m_push_constants_size = sizeof(T);
    }

    u32 get_push_constants_size() { return (m_descriptor_count * sizeof(u16)) + m_push_constants_size; }

    GraphicsPipelineInfo m_graphics_pipeline_info = {};
    eastl::vector<ShaderCompileDesc> m_compile_descs = {};
    eastl::vector<eastl::string> m_definitions = {};
    eastl::vector<eastl::string> m_include_dirs = {};
    u32 m_descriptor_count = 0;
    u32 m_push_constants_size = 0;
};

LR_HANDLE(ShaderID, usize);
LR_HANDLE(PipelineID, usize);
LR_HANDLE(PipelineInfoID, usize);

struct PipelineManager {
    void init(Device *device);
    virtual ~PipelineManager();

    struct PipelineInfo {
        eastl::vector<ShaderReflectionData> m_reflections = {};
        eastl::vector<ShaderID> m_bound_shaders = {};
        PipelineID m_pipeline_id = {};
    };

    PipelineInfoID compile_pipeline(PipelineCompileInfo &compile_info, PipelineAttachmentInfo &attachment_info, PipelineLayout *layout);

    PipelineInfo *get_pipeline_info(PipelineInfoID pipeline_id);
    Pipeline *get_pipeline(PipelineID pipeline_id);

    ResourcePool<Shader, ShaderID> m_shaders = {};
    ResourcePool<Pipeline, PipelineID> m_pipelines = {};
    ResourcePool<PipelineInfo, PipelineInfoID> m_pipeline_infos = {};

    Device *m_device = nullptr;
};

}  // namespace lr::Graphics
