#pragma once

#include "VulkanEnums.hh"

#include "Engine/Core/Handle.hh"

namespace lr {
enum class BufferID : u32 { Invalid = ~0_u32 };
enum class ImageID : u32 { Invalid = ~0_u32 };
enum class ImageViewID : u32 { Invalid = ~0_u32 };
enum class SamplerID : u32 { Invalid = ~0_u32 };
enum class PipelineID : u32 { Invalid = ~0_u32 };
enum class PipelineLayoutID : u32 { None = 0 };

using Buffer_H = Handle<struct Buffer>;
using Image_H = Handle<struct Image>;
using ImageView_H = Handle<struct ImageView>;
using Sampler_H = Handle<struct Sampler>;

using Shader_H = Handle<struct Shader>;
using Pipeline_H = Handle<struct Pipeline>;

using Semaphore_H = Handle<struct Semaphore>;
using QueryPool_H = Handle<struct QueryPool>;
using CommandList_H = Handle<struct CommandList>;
using CommandQueue_H = Handle<struct CommandQueue>;

using Surface_H = Handle<struct Surface>;
using SwapChain_H = Handle<struct SwapChain>;
using Device_H = Handle<struct Device>;

}  // namespace lr

namespace lr::vk {
union ColorClearValue {
    ColorClearValue() = default;
    ColorClearValue(f32 r, f32 g, f32 b, f32 a)
        : color_f32({ r, g, b, a }) {};
    ColorClearValue(u32 r, u32 g, u32 b, u32 a)
        : color_u32({ r, g, b, a }) {};
    ColorClearValue(i32 r, i32 g, i32 b, i32 a)
        : color_i32({ r, g, b, a }) {};
    ColorClearValue(const glm::vec4 &val)
        : color_f32(val) {};
    ColorClearValue(const glm::uvec4 &val)
        : color_u32(val) {};
    ColorClearValue(const glm::ivec4 &val)
        : color_i32(val) {};

    glm::vec4 color_f32 = {};
    glm::uvec4 color_u32;
    glm::ivec4 color_i32;
};

struct DepthClearValue {
    DepthClearValue() = default;
    DepthClearValue(f32 depth_)
        : depth(depth_) {};
    DepthClearValue(f32 depth_, u32 stencil_)
        : depth(depth_),
          stencil(stencil_) {};

    f32 depth = 0.0f;
    u32 stencil = 0;
};

// Safe to cast VK type
union ClearValue {
    ColorClearValue color = {};
    DepthClearValue depth;
};

// Safe to cast VK type
struct Offset2D {
    i32 x = 0;
    i32 y = 0;
};

// Safe to cast VK type
struct Offset3D {
    i32 x = 0;
    i32 y = 0;
    i32 z = 0;
};

// Safe to cast VK type
struct Extent2D {
    u32 width = 0;
    u32 height = 0;

    bool is_zero() const { return width == 0 && height == 0; }
};

// Safe to cast VK type
struct Extent3D {
    u32 width = 0;
    u32 height = 0;
    u32 depth = 0;

    bool is_zero() const { return width == 0 && height == 0 && depth == 0; }
};

// Safe to cast VK type
struct Viewport {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
    f32 depth_min = 0.0f;
    f32 depth_max = 1.0f;
};

// Safe to cast VK type
struct Rect2D {
    Offset2D offset = {};
    Extent2D extent = {};
};

// Subresource range, it's in the name
struct ImageSubresourceRange {
    ImageAspectFlag aspect_flags = ImageAspectFlag::Color;
    u32 base_mip = 0;
    u32 mip_count = 1;
    u32 base_slice = 0;
    u32 slice_count = 1;
};

// Subresource info used for layers(slices)
struct ImageSubresourceLayers {
    ImageAspectFlag aspect_flags = ImageAspectFlag::Color;
    u32 target_mip = 0;
    u32 base_slice = 0;
    u32 slice_count = 1;
};

struct PushConstantRange {
    ShaderStageFlag stage = ShaderStageFlag::All;
    u32 offset = 0;
    u32 size = 0;
};

struct StencilFaceOp {
    StencilOp pass = {};
    StencilOp fail = {};
    StencilOp depth_fail = {};
    CompareOp compare_func = {};
    ColorMask compare_mask = ColorMask::None;
    ColorMask write_mask = ColorMask::None;
    u32 reference = 0;
};

struct VertexLayoutBindingInfo {
    u32 binding = 0;
    u32 stride = 0;
    VertexInputRate input_rate = VertexInputRate::Vertex;
};

struct VertexAttribInfo {
    Format format = Format::Undefined;
    u32 location = 0;
    u32 binding = 0;
    u32 offset = 0;
};

struct RasterizerStateInfo {
    bool enable_depth_clamp = false;
    bool enable_depth_bias = false;
    bool enable_wireframe = false;
    bool front_face_ccw = false;
    f32 depth_slope_factor = 0.0;
    f32 depth_bias_clamp = 0.0;
    f32 depth_bias_factor = 0.0;
    f32 line_width = 1.0;
    CullMode cull_mode = {};
    PrimitiveType primitive_type = PrimitiveType::TriangleList;
};

struct MultisampleStateInfo {
    u32 multisample_bit_count = 1;
    bool enable_alpha_to_coverage = false;
    bool enable_alpha_to_one = false;
};

struct DepthStencilStateInfo {
    bool enable_depth_test = false;
    bool enable_depth_write = false;
    bool enable_depth_bounds_test = false;
    bool enable_stencil_test = false;
    CompareOp depth_compare_op = {};
    StencilFaceOp stencil_front_face_op = {};
    StencilFaceOp stencil_back_face_op = {};
};

struct PipelineColorBlendAttachment {
    b32 blend_enabled = false;
    BlendFactor src_blend = BlendFactor::SrcAlpha;
    BlendFactor dst_blend = BlendFactor::InvSrcAlpha;
    BlendOp blend_op = BlendOp::Add;
    BlendFactor src_blend_alpha = BlendFactor::One;
    BlendFactor dst_blend_alpha = BlendFactor::InvSrcAlpha;
    BlendOp blend_op_alpha = BlendOp::Add;
    ColorMask write_mask = ColorMask::RGBA;
};

struct BufferCopyRegion {
    u64 src_offset = 0;
    u64 dst_offset = 0;
    u64 size = 0;
};

struct ImageCopyRegion {
    u64 buffer_offset = 0;
    u32 buffer_row_length = 0;
    u32 buffer_image_height = 0;
    ImageSubresourceLayers image_subresource_layer = {};
    Offset3D image_offset = {};
    Extent3D image_extent = {};
};

struct ImageBlit {
    ImageSubresourceLayers src_subresource = {};
    Offset3D src_offsets[2] = {};
    ImageSubresourceLayers dst_subresource = {};
    Offset3D dst_offsets[2] = {};
};

struct ImageBarrier {
    PipelineStage src_stage = PipelineStage::None;
    MemoryAccess src_access = MemoryAccess::None;
    PipelineStage dst_stage = PipelineStage::None;
    MemoryAccess dst_access = MemoryAccess::None;
    ImageLayout old_layout = ImageLayout::Undefined;
    ImageLayout new_layout = ImageLayout::Undefined;
    u32 src_queue_family = ~0_u32;
    u32 dst_queue_family = ~0_u32;
    ImageID image_id = ImageID::Invalid;
    ImageSubresourceRange subresource_range = {};
};

struct MemoryBarrier {
    PipelineStage src_stage = PipelineStage::None;
    MemoryAccess src_access = MemoryAccess::None;
    PipelineStage dst_stage = PipelineStage::None;
    MemoryAccess dst_access = MemoryAccess::None;
};

struct RenderingAttachmentInfo {
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout image_layout = ImageLayout::Undefined;
    AttachmentLoadOp load_op = AttachmentLoadOp::DontCare;
    AttachmentStoreOp store_op = AttachmentStoreOp::DontCare;
    ClearValue clear_value = {};
};

struct ImageDescriptorInfo {
    ImageViewID image_view = ImageViewID::Invalid;
    ImageLayout image_layout = ImageLayout::Undefined;
};

struct BufferDescriptorInfo {
    BufferID buffer = BufferID::Invalid;
    u64 offset = 0;
    u64 range = ~0ULL;
};

struct MemoryRequirements {
    u64 size = 0;
    u64 alignment = 0;
    u32 memory_type_bits = 0;
};
}  // namespace lr::vk

namespace lr {
/////////////////////////////////
// DEVICE RESOURCES

struct Buffer : Handle<Buffer> {
    static auto create(
        Device_H, vk::BufferUsage usage, u64 size, vk::MemoryAllocationUsage memory_usage, vk::MemoryAllocationFlag memory_flags)
        -> std::expected<BufferID, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Buffer &;

    auto data_size() -> u64;
    auto device_address() -> u64;
    auto host_ptr() -> u8 *;
};

struct Image : Handle<Image> {
    static auto create(
        Device_H,
        vk::ImageUsage usage,
        vk::Format format,
        vk::ImageType type,
        vk::Extent3D extent,
        vk::ImageAspectFlag aspect_flags = vk::ImageAspectFlag::Color,
        u32 slice_count = 1,
        u32 mip_count = 1) -> std::expected<ImageID, vk::Result>;
    static auto create_for_swap_chain(Device_H, vk::Format format, vk::Extent3D extent) -> std::expected<ImageID, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Image &;

    auto format() -> vk::Format;
    auto extent() -> vk::Extent3D;
    auto subresource_range() -> vk::ImageSubresourceRange;
};

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageView : Handle<ImageView> {
    static auto create(Device_H, ImageID image_id, vk::ImageViewType type, vk::ImageSubresourceRange subresource_range)
        -> std::expected<ImageViewID, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> ImageView &;

    auto format() -> vk::Format;
    auto aspect_flags() -> vk::ImageAspectFlag;
    auto subresource_range() -> vk::ImageSubresourceRange;
};

struct Sampler : Handle<Sampler> {
    static auto create(
        Device_H,
        vk::Filtering min_filter,
        vk::Filtering mag_filter,
        vk::Filtering mip_filter,
        vk::SamplerAddressMode addr_u,
        vk::SamplerAddressMode addr_v,
        vk::SamplerAddressMode addr_w,
        vk::CompareOp compare_op,
        f32 max_anisotropy = 0,
        f32 mip_lod_bias = 0,
        f32 min_lod = 0,
        f32 max_lod = 1000.0f,
        bool use_anisotropy = false) -> std::expected<SamplerID, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Sampler &;
};

/////////////////////////////////
// PIPELINE

struct Shader : Handle<Shader> {
    static auto create(Device_H, const std::vector<u32> &ir, vk::ShaderStageFlag stage) -> std::expected<Shader, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Shader &;
};

struct GraphicsPipelineInfo {
    std::vector<vk::Format> color_attachment_formats = {};
    ls::option<vk::Format> depth_attachment_format = ls::nullopt;
    ls::option<vk::Format> stencil_attachment_format = ls::nullopt;

    std::vector<Shader_H> shaders = {};
    // Vertex Input State
    std::vector<vk::VertexAttribInfo> vertex_attrib_infos = {};

    vk::RasterizerStateInfo rasterizer_state = {};
    vk::MultisampleStateInfo multisample_state = {};
    vk::DepthStencilStateInfo depth_stencil_state = {};
    // Color Blend Attachment State
    std::vector<vk::PipelineColorBlendAttachment> blend_attachments = {};
    glm::vec4 blend_constants = {};

    PipelineLayoutID layout_id = PipelineLayoutID::None;
};

struct ComputePipelineInfo {
    Device_H device = {};
    Shader_H shader = {};
    PipelineLayoutID layout_id = PipelineLayoutID::None;
    std::string name = {};
};

struct Pipeline : Handle<Pipeline> {
    static auto create(Device_H, const GraphicsPipelineInfo &info) -> std::expected<PipelineID, vk::Result>;
    static auto create(Device_H, const ComputePipelineInfo &info) -> std::expected<PipelineID, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Pipeline &;

    auto bind_point() -> vk::PipelineBindPoint;
    auto layout_id() -> PipelineLayoutID;
};

/////////////////////////////////
// COMMAND LIST

struct Semaphore : Handle<Semaphore> {
    static auto create(Device_H, const ls::option<u64> &initial_value) -> std::expected<Semaphore, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> Semaphore &;

    auto wait(u64 wait_value) -> void;
    auto advance() -> u64;
    auto value() -> u64;
    auto counter() -> u64;
};

struct QueryPool : Handle<QueryPool> {
    static auto create(Device_H, u32 query_count) -> std::expected<QueryPool, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> QueryPool &;

    auto get_results(u32 first_query, u32 count, ls::span<u64> time_stamps) -> void;
};

struct RenderingBeginInfo {
    vk::Rect2D render_area = {};
    ls::span<vk::RenderingAttachmentInfo> color_attachments = {};
    std::optional<vk::RenderingAttachmentInfo> depth_attachment = std::nullopt;
    std::optional<vk::RenderingAttachmentInfo> stencil_attachment = std::nullopt;
};

struct CommandList : Handle<CommandList> {
    friend CommandQueue;

    auto reset_query_pool(QueryPool_H, u32 first_query, u32 query_count) -> void;
    auto write_query_pool(QueryPool_H, vk::PipelineStage pipeline_stage, u32 query_index) -> void;

    auto image_transition(const vk::ImageBarrier &barrier) -> void;
    auto memory_barrier(const vk::MemoryBarrier &barrier) -> void;
    auto set_barriers(ls::span<vk::MemoryBarrier> memory_barriers, ls::span<vk::ImageBarrier> image_barriers) -> void;

    auto copy_buffer_to_buffer(BufferID src_id, BufferID dst_id, ls::span<vk::BufferCopyRegion> regions) -> void;
    auto copy_buffer_to_image(BufferID src_id, ImageID dst_id, vk::ImageLayout layout, ls::span<vk::ImageCopyRegion> regions) -> void;
    auto blit_image(
        ImageID src_id,
        vk::ImageLayout src_layout,
        ImageID dst_id,
        vk::ImageLayout dst_layout,
        vk::Filtering filter,
        ls::span<vk::ImageBlit> blits) -> void;

    auto begin_rendering(const RenderingBeginInfo &info) -> void;
    auto end_rendering() -> void;
    auto set_pipeline(PipelineID pipeline_id) -> void;
    auto set_push_constants(void *data, u32 data_size, u32 data_offset) -> void;
    auto set_descriptor_set() -> void;
    auto set_vertex_buffer(BufferID buffer_id, u64 offset = 0, u32 first_binding = 0, u32 binding_count = 1) -> void;
    auto set_index_buffer(BufferID buffer_id, u64 offset = 0, bool use_u16 = false);
    auto set_viewport(const vk::Viewport &viewport) -> void;
    auto set_scissors(const vk::Rect2D &rect) -> void;

    auto draw(u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0) -> void;
    auto draw_indexed(u32 index_count, u32 first_index = 0, i32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0) -> void;
    auto dispatch(u32 group_x, u32 group_y, u32 group_z) -> void;
};

struct CommandBatcher {
    constexpr static usize MAX_MEMORY_BARRIERS = 16;
    constexpr static usize MAX_IMAGE_BARRIERS = 16;

    CommandList &command_list;

    CommandBatcher(CommandList &command_list_);
    ~CommandBatcher();

    auto insert_memory_barrier(const vk::MemoryBarrier &barrier) -> void;
    auto insert_image_barrier(const vk::ImageBarrier &barrier) -> void;
    auto flush_barriers() -> void;

    ls::static_vector<vk::MemoryBarrier, MAX_MEMORY_BARRIERS> memory_barriers = {};
    ls::static_vector<vk::ImageBarrier, MAX_IMAGE_BARRIERS> image_barriers = {};
};

struct CommandQueue : Handle<CommandQueue> {
    static auto create(Device_H, vk::CommandType type) -> std::expected<CommandQueue, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> CommandQueue;

    auto semaphore() -> Semaphore;

    auto defer(ls::span<BufferID> buffer_ids) -> void;
    auto defer(ls::span<ImageID> image_ids) -> void;
    auto defer(ls::span<ImageViewID> image_view_ids) -> void;
    auto defer(ls::span<SamplerID> sampler_ids) -> void;
    auto collect_garbage() -> void;

    auto begin(std::source_location LOC = std::source_location::current()) -> CommandList;
    auto end(CommandList &cmd_list) -> void;

    auto submit(ls::span<Semaphore> wait_semas, ls::span<Semaphore> signal_semas) -> vk::Result;
    auto acquire(SwapChain swap_chain, Semaphore acquire_sema) -> std::expected<u32, vk::Result>;
    auto present(SwapChain swap_chain, Semaphore present_sema, u32 image_id) -> vk::Result;
    auto wait() -> void;
};

/////////////////////////////////
// DEVICE

struct Surface : Handle<Surface> {
    static auto create(Device_H, void *handle) -> std::expected<Surface, vk::Result>;
};

struct SwapChain : Handle<SwapChain> {
    static auto create(Device_H, Surface_H surface, vk::Extent2D extent) -> std::expected<SwapChain, vk::Result>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> SwapChain &;

    auto format() -> vk::Format;
    auto extent() -> vk::Extent3D;
    auto semaphores(usize i) -> std::pair<Semaphore, Semaphore>;
    auto get_images() -> std::pair<std::vector<ImageID>, std::vector<ImageViewID>>;
};

struct GPUAllocation {
    BufferID buffer_id = BufferID::Invalid;
    u8 *ptr = nullptr;
    u64 offset = 0;
    u64 size = 0;
};

struct TransferManager : Handle<TransferManager> {
    // Transfer Manager uses offset allocator (by sebbbi), it is same as TLSF.
    // I wrote my TLSF impl. so long ago, I basically forgot about it.
    // Better to use something working.
    //
    using NodeID = u16;
    static constexpr u32 NUM_TOP_BINS = 32;
    static constexpr u32 BINS_PER_LEAF = 8;
    static constexpr u32 TOP_BINS_INDEX_SHIFT = 3;
    static constexpr u32 LEAF_BINS_INDEX_MASK = 0x7;
    static constexpr u32 NUM_LEAF_BINS = NUM_TOP_BINS * BINS_PER_LEAF;

    auto static create(Device_H, u32 capacity = ls::mib_to_bytes(32), u32 max_allocs = 128 * 1024) -> TransferManager;
    auto destroy() -> void;

    auto allocate(u64 size, u64 alignment = 16) -> ls::option<GPUAllocation>;
    auto collect_garbage() -> void;
    auto semaphore() -> Semaphore;
};

enum class DeviceFeature : u64 {
    None = 0,
    DescriptorBuffer = 1 << 0,
    MemoryBudget = 1 << 1,
    QueryTimestamp = 1 << 2,
};
consteval void enable_bitmask(DeviceFeature);

struct Device : Handle<Device> {
    static auto create(usize frame_count) -> std::expected<Device, vk::Result>;
    auto destroy() -> void;

    auto frame_count() -> usize;
    auto frame_sema() -> Semaphore;
    auto queue(vk::CommandType type) -> CommandQueue;
    auto transfer_manager() -> TransferManager;
    auto wait() -> void;

    auto new_frame() -> usize;
    auto end_frame() -> void;

    auto buffer(BufferID) -> Buffer;
    auto image(ImageID) -> Image;
    auto image_view(ImageViewID) -> ImageView;
    auto sampler(SamplerID) -> Sampler;
    auto pipeline(PipelineID) -> Pipeline;

    auto upload(ImageID dst_image_id, void *data, u64 data_size, vk::ImageLayout new_layout) -> void;

    auto destroy_buffer(BufferID) -> void;
    auto destroy_image(ImageID) -> void;
    auto destroy_image_view(ImageViewID) -> void;
    auto destroy_sampler(SamplerID) -> void;
    auto destroy_pipeline(PipelineID) -> void;

    struct Limits {
        constexpr static u32 FrameCount = 3;
        constexpr static u64 PersistentBufferSize = ls::mib_to_bytes(128);
    };
};

}  // namespace lr
