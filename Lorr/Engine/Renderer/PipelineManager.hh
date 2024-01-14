#pragma once

#include "Graphics/Device.hh"
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

LR_HANDLE(ShaderID, usize);
LR_HANDLE(PipelineID, usize);
LR_HANDLE(PipelineInfoID, usize);

struct PipelineManager
{
    void init(Device *device);
    virtual ~PipelineManager();

    struct PipelineInfo
    {
        eastl::vector<ShaderReflectionData> m_reflections = {};
        eastl::vector<ShaderID> m_bound_shaders = {};
        PipelineID m_pipeline_id = {};
    };

    PipelineInfoID compile_pipeline(PipelineCompileInfo &compile_info, PipelineAttachmentInfo &attachment_info, PipelineLayout &layout);

    PipelineInfo *get_pipeline_info(PipelineInfoID pipeline_id);
    Pipeline *get_pipeline(PipelineID pipeline_id);

    ResourcePool<Shader, ShaderID> m_shaders = {};
    ResourcePool<Pipeline, PipelineID> m_pipelines = {};
    ResourcePool<PipelineInfo, PipelineInfoID> m_pipeline_infos = {};

    Device *m_device = nullptr;
};

struct DescriptorManagerDesc
{
    Device *m_device = nullptr;
    eastl::span<DescriptorLayoutElement> m_persistent_descriptors = {};
    u32 m_frame_count = 0;
    eastl::span<DescriptorLayoutElement> m_dynamic_descriptors = {};
};

struct DescriptorManager
{
    void init(DescriptorManagerDesc *desc);
    ~DescriptorManager();

    void update_descriptor_set(u32 target_set, u32 target_slot, ImageView *image_view, ImageLayout layout, DescriptorType descriptor_type);
    void bind_all(CommandList *list, CommandType type, u32 frame_idx);

    auto is_buffered() { return m_device->is_feature_supported(DeviceFeature::DescriptorBuffer); }
    auto &get_layout() { return m_pipeline_layout; }
    DescriptorSet &get_descriptor_set(u32 layout_idx);

    eastl::array<u32, (usize)DescriptorType::Count> m_persistent_set_indexes = {};
    u32 m_dynamic_sets_stride = 0;
    eastl::vector<DescriptorSet> m_dynamic_sets = {};
    eastl::vector<DescriptorSet> m_persistent_sets = {};
    DescriptorPool m_descriptor_pool = {};
    Buffer m_dynamic_buffer = {};
    Buffer m_persistent_buffer = {};
    PipelineLayout m_pipeline_layout = nullptr;

    Device *m_device = nullptr;
};

}  // namespace lr::Graphics