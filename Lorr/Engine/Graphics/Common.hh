#pragma once

#include "Vulkan.hh"  // IWYU pragma: export

namespace lr::Graphics
{
template<typename StartT>
struct chained_struct
{
    chained_struct(StartT &start) { m_cur = reinterpret_cast<VkBaseOutStructure *>(&start); }

    template<typename T>
    chained_struct set_next(T &next)
    {
        m_cur->pNext = reinterpret_cast<VkBaseOutStructure *>(&next);
        return *this;
    }

    VkBaseOutStructure *m_cur = nullptr;
};

enum class DeviceFeature : u64
{
    None = 0,
    DescriptorBuffer = 1 << 0,
    MemoryBudget = 1 << 1,
};
LR_TYPEOP_ARITHMETIC_INT(DeviceFeature, DeviceFeature, &);
LR_TYPEOP_ARITHMETIC(DeviceFeature, DeviceFeature, |);
LR_TYPEOP_ASSIGNMENT(DeviceFeature, DeviceFeature, |);

enum class APIResult : i32
{
    Success = VK_SUCCESS,
    NotReady = VK_NOT_READY,
    TimeOut = VK_TIMEOUT,
    Incomplete = VK_INCOMPLETE,
    OutOfHostMem = VK_ERROR_OUT_OF_HOST_MEMORY,
    OutOfDeviceMem = VK_ERROR_OUT_OF_DEVICE_MEMORY,
    InitFailed = VK_ERROR_INITIALIZATION_FAILED,
    DeviceLost = VK_ERROR_DEVICE_LOST,
    MemMapFailed = VK_ERROR_MEMORY_MAP_FAILED,
    LayerNotPresent = VK_ERROR_LAYER_NOT_PRESENT,
    ExtNotPresent = VK_ERROR_EXTENSION_NOT_PRESENT,
    FeatureNotPresent = VK_ERROR_FEATURE_NOT_PRESENT,
    IncompatibleDriver = VK_ERROR_INCOMPATIBLE_DRIVER,
    TooManyObjects = VK_ERROR_TOO_MANY_OBJECTS,
    FormatNotSupported = VK_ERROR_FORMAT_NOT_SUPPORTED,
    FragmentedPool = VK_ERROR_FRAGMENTED_POOL,
    Unknown = VK_ERROR_UNKNOWN,
    Fragmentation = VK_ERROR_FRAGMENTATION,
    SurfaceLost = VK_ERROR_SURFACE_LOST_KHR,
    WindowInUse = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
    Suboptimal = VK_SUBOPTIMAL_KHR,
    OutOfDate = VK_ERROR_OUT_OF_DATE_KHR,
    InvalidExternalHandle = VK_ERROR_INVALID_EXTERNAL_HANDLE,
    OutOfPoolMem = VK_ERROR_OUT_OF_POOL_MEMORY,

    HanldeNotInitialized = VK_RESULT_MAX_ENUM,
};

constexpr static APIResult CHECK(VkResult vkr, [[maybe_unused]] std::initializer_list<APIResult> allowed_checks = {})
{
    auto result = static_cast<APIResult>(vkr);
#if _DEBUG
    if (result != APIResult::Success)
        for (auto &a : allowed_checks)
            if (a == result)
                return result;

    assert(result == APIResult::Success);
#endif

    return result;
}

enum class PresentMode : u32
{
    Immediate = VK_PRESENT_MODE_IMMEDIATE_KHR,
    Mailbox = VK_PRESENT_MODE_MAILBOX_KHR,
    Fifo = VK_PRESENT_MODE_FIFO_KHR,
    FifoRelazed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};

/// BUFFER ---------------------------- ///

enum class BufferUsage : u64
{
    None = 0,
    Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    SamplerDescriptor = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
    ResourceDescriptor = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    BufferDeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
};
LR_TYPEOP_ARITHMETIC_INT(BufferUsage, BufferUsage, &);
LR_TYPEOP_ARITHMETIC(BufferUsage, BufferUsage, |);
LR_TYPEOP_ASSIGNMENT(BufferUsage, BufferUsage, |);

/// COMMAND ---------------------------- ///

enum class CommandType : u32
{
    Graphics = 0,
    Compute,
    Transfer,
    Count,
};

enum class CommandTypeMask : u32
{
    Graphics = 1 << static_cast<u32>(CommandType::Graphics),
    Compute = 1 << static_cast<u32>(CommandType::Compute),
    Transfer = 1 << static_cast<u32>(CommandType::Transfer),
};
LR_TYPEOP_ARITHMETIC(CommandTypeMask, CommandTypeMask, |);
LR_TYPEOP_ARITHMETIC_INT(CommandTypeMask, CommandTypeMask, &);
LR_TYPEOP_ASSIGNMENT(CommandTypeMask, CommandTypeMask, |);

enum class CommandAllocatorFlag : u32
{
    Transient = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    ResetCommandBuffer = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    Protected = VK_COMMAND_POOL_CREATE_PROTECTED_BIT,
};
LR_TYPEOP_ARITHMETIC(CommandAllocatorFlag, CommandAllocatorFlag, |);
LR_TYPEOP_ARITHMETIC_INT(CommandAllocatorFlag, CommandAllocatorFlag, &);

/// DESCRIPTOR ---------------------------- ///

enum class DescriptorType : u32
{
    Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
    SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,    // Read only image
    UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // Read only buffer
    StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,    // RW image
    StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RW Buffer

    Count = 5,
};

enum class DescriptorSetLayoutFlag : u32
{
    None = 0,
    DescriptorBuffer = 1 << 0,
    EmbeddedSamplers = 1 << 1,
};
LR_TYPEOP_ARITHMETIC(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, &);

enum class DescriptorBindingFlag : u32
{
    None = 0,
    UpdateAfterBind = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
    UpdateUnusedWhilePending = VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
    PartiallyBound = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VariableDescriptorCount = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
};
LR_TYPEOP_ARITHMETIC(DescriptorBindingFlag, DescriptorBindingFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorBindingFlag, DescriptorBindingFlag, &);

enum class DescriptorPoolFlag : u32
{
    None = 0,
    FreeDescriptorSet = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    UpdateAfterBind = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
};
LR_TYPEOP_ARITHMETIC(DescriptorPoolFlag, DescriptorPoolFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorPoolFlag, DescriptorPoolFlag, &);

/// IMAGE ---------------------------- ///

enum class Format : u32
{
    // Format: CB_T
    // C = Component, B = bit count, T = type

    // Generic types -- component dependant types
    Unknown = VK_FORMAT_UNDEFINED,

    R32_SFLOAT = VK_FORMAT_R32_SFLOAT,
    R32_SINT = VK_FORMAT_R32_SINT,
    R32_UINT = VK_FORMAT_R32_UINT,

    R32G32_SFLOAT = VK_FORMAT_R32G32_SFLOAT,
    R32G32_SINT = VK_FORMAT_R32G32_SINT,
    R32G32_UINT = VK_FORMAT_R32G32_UINT,

    R32G32B32_SFLOAT = VK_FORMAT_R32G32B32_SFLOAT,
    R32G32B32_SINT = VK_FORMAT_R32G32B32_SINT,
    R32G32B32_UINT = VK_FORMAT_R32G32B32_UINT,

    R8G8B8A8_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
    R8G8B8A8_INT = VK_FORMAT_R8G8B8A8_SINT,
    R8G8B8A8_UINT = VK_FORMAT_R8G8B8A8_UINT,
    R8G8B8A8_SRGB = VK_FORMAT_R8G8B8A8_SRGB,

    R16G16B16A16_SFLOAT = VK_FORMAT_R16G16B16A16_SFLOAT,
    R16G16B16A16_SINT = VK_FORMAT_R16G16B16A16_SINT,
    R16G16B16A16_UINT = VK_FORMAT_R16G16B16A16_UINT,

    R32G32B32A32_SFLOAT = VK_FORMAT_R32G32B32A32_SFLOAT,
    R32G32B32A32_SINT = VK_FORMAT_R32G32B32A32_SINT,
    R32G32B32A32_UINT = VK_FORMAT_R32G32B32A32_UINT,

    B8G8R8A8_UNORM = VK_FORMAT_B8G8R8A8_UNORM,
    B8G8R8A8_INT = VK_FORMAT_B8G8R8A8_SINT,
    B8G8R8A8_UINT = VK_FORMAT_B8G8R8A8_UINT,
    B8G8R8A8_SRGB = VK_FORMAT_B8G8R8A8_SRGB,

    D32_SFLOAT = VK_FORMAT_D32_SFLOAT,
    D24_SFLOAT_S8_UINT = VK_FORMAT_D24_UNORM_S8_UINT,

    // Vector types -- must index to generic types
    Float = R32_SFLOAT,
    Int = R32_SINT,
    UInt = R32_UINT,
    Vec2 = R32G32_SFLOAT,
    Vec2I = R32G32_SINT,
    Vec2U = R32G32_UINT,
    Vec3 = R32G32B32_SFLOAT,
    Vec3I = R32G32B32_SINT,
    Vec3U = R32G32B32_UINT,
    Vec4 = R32G32B32A32_SFLOAT,
    Vec4I = R32G32B32A32_SINT,
    Vec4U = R32G32B32A32_SINT,
};

constexpr static u32 format_to_size(Format format)
{
    constexpr u32 format_size_lut[] = {
        [(u32)Format::Unknown] = 0,
        [(u32)Format::R32_SFLOAT] = sizeof(u32),
        [(u32)Format::R32_SINT] = sizeof(u32),
        [(u32)Format::R32_UINT] = sizeof(u32),
        [(u32)Format::R32G32_SFLOAT] = sizeof(u32) * 2,
        [(u32)Format::R32G32_SINT] = sizeof(u32) * 2,
        [(u32)Format::R32G32_UINT] = sizeof(u32) * 2,
        [(u32)Format::R32G32B32_SFLOAT] = sizeof(u32) * 3,
        [(u32)Format::R32G32B32_SINT] = sizeof(u32) * 3,
        [(u32)Format::R32G32B32_UINT] = sizeof(u32) * 3,
        [(u32)Format::R8G8B8A8_UNORM] = sizeof(u8) * 4,
        [(u32)Format::R8G8B8A8_INT] = sizeof(u8) * 4,
        [(u32)Format::R8G8B8A8_UINT] = sizeof(u8) * 4,
        [(u32)Format::R8G8B8A8_SRGB] = sizeof(u8) * 4,
        [(u32)Format::R16G16B16A16_SFLOAT] = sizeof(u16) * 4,
        [(u32)Format::R16G16B16A16_SINT] = sizeof(u16) * 4,
        [(u32)Format::R16G16B16A16_UINT] = sizeof(u16) * 4,
        [(u32)Format::R32G32B32A32_SFLOAT] = sizeof(u32) * 4,
        [(u32)Format::R32G32B32A32_SINT] = sizeof(u32) * 4,
        [(u32)Format::R32G32B32A32_UINT] = sizeof(u32) * 4,
        [(u32)Format::B8G8R8A8_UNORM] = sizeof(u8) * 4,
        [(u32)Format::B8G8R8A8_INT] = sizeof(u8) * 4,
        [(u32)Format::B8G8R8A8_UINT] = sizeof(u8) * 4,
        [(u32)Format::B8G8R8A8_SRGB] = sizeof(u8) * 4,
        [(u32)Format::D32_SFLOAT] = sizeof(u32),
        [(u32)Format::D24_SFLOAT_S8_UINT] = sizeof(u32),
    };

    return format_size_lut[(u32)format];
}

enum class ImageUsage : u32
{
    Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    Storage = VK_IMAGE_USAGE_STORAGE_BIT,
    Present = 1 << 29,  // Virtual flag
};
LR_TYPEOP_ARITHMETIC_INT(ImageUsage, ImageUsage, &);

enum class ImageLayout : u32
{
    Undefined = VK_IMAGE_LAYOUT_UNDEFINED,
    Present = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    ColorAttachment = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    DepthStencilAttachment = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    ColorReadOnly = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    DepthStencilReadOnly = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    TransferSrc = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    TransferDst = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    General = VK_IMAGE_LAYOUT_GENERAL,
};

enum class ImageType : u32
{
    View1D = VK_IMAGE_TYPE_1D,
    View2D = VK_IMAGE_TYPE_2D,
    View3D = VK_IMAGE_TYPE_3D,
};

enum class ImageViewType : u32
{
    View1D = VK_IMAGE_VIEW_TYPE_1D,
    View2D = VK_IMAGE_VIEW_TYPE_2D,
    View3D = VK_IMAGE_VIEW_TYPE_3D,
    Cube = VK_IMAGE_VIEW_TYPE_CUBE,
    Array1D = VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    Array2D = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    CubeArray = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
};

enum class ImageAspect : u32
{
    Color = VK_IMAGE_ASPECT_COLOR_BIT,
    Depth = VK_IMAGE_ASPECT_DEPTH_BIT,
    Stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
    DepthStencil = Depth | Stencil,
};

enum class ImageComponentSwizzle : u32
{
    Identity = VK_COMPONENT_SWIZZLE_IDENTITY,
    Zero = VK_COMPONENT_SWIZZLE_ZERO,
    One = VK_COMPONENT_SWIZZLE_ONE,
    R = VK_COMPONENT_SWIZZLE_R,
    G = VK_COMPONENT_SWIZZLE_G,
    B = VK_COMPONENT_SWIZZLE_B,
    A = VK_COMPONENT_SWIZZLE_A,
};

/// PIPELINE ---------------------------- ///

enum class PipelineBindPoint : u32
{
    Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    Compute = VK_PIPELINE_BIND_POINT_COMPUTE,
};

enum class PipelineStage : u64
{
    None = VK_PIPELINE_STAGE_2_NONE,
    TopOfPipe = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,

    /// ---- IN ORDER ----
    DrawIndirect = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,

    /// GRAPHICS PIPELINE
    VertexAttribInput = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
    IndexAttribInput = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
    VertexShader = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    TessellationControl = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
    TessellationEvaluation = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
    PixelShader = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    EarlyPixelTests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    LatePixelTests = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    ColorAttachmentOutput = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    AllGraphics = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,

    /// COMPUTE PIPELINE
    ComputeShader = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,

    /// TRANSFER PIPELINE
    Host = VK_PIPELINE_STAGE_2_HOST_BIT,
    Copy = VK_PIPELINE_STAGE_2_COPY_BIT,
    Bilt = VK_PIPELINE_STAGE_2_BLIT_BIT,
    Resolve = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
    Clear = VK_PIPELINE_STAGE_2_CLEAR_BIT,
    AllTransfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT,

    /// OTHER STAGES
    AllCommands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    BottomOfPipe = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
};
LR_TYPEOP_ARITHMETIC(PipelineStage, PipelineStage, |);
LR_TYPEOP_ARITHMETIC_INT(PipelineStage, PipelineStage, &);

enum class MemoryAccess : u64
{
    None = VK_ACCESS_2_NONE,
    IndirectRead = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
    VertexAttribRead = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
    IndexAttribRead = VK_ACCESS_2_INDEX_READ_BIT,
    InputAttachmentRead = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
    ShaderRead = VK_ACCESS_2_SHADER_READ_BIT,
    UniformRead = VK_ACCESS_2_UNIFORM_READ_BIT,
    SampledRead = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    StorageRead = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
    StorageWrite = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    ColorAttachmentRead = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    ColorAttachmentWrite = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    DepthStencilRead = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    DepthStencilWrite = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    TransferRead = VK_ACCESS_2_TRANSFER_READ_BIT,
    TransferWrite = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    HostRead = VK_ACCESS_2_HOST_READ_BIT,
    HostWrite = VK_ACCESS_2_HOST_WRITE_BIT,
    Read = VK_ACCESS_2_MEMORY_READ_BIT,
    Write = VK_ACCESS_2_MEMORY_WRITE_BIT,
    ReadWrite = Read | Write,
};
LR_TYPEOP_ARITHMETIC(MemoryAccess, MemoryAccess, |);
LR_TYPEOP_ARITHMETIC_INT(MemoryAccess, MemoryAccess, &);

enum class PrimitiveType : u32
{
    PointList = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    LineList = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    TriangleList = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    Patch = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
};

enum class CullMode : u32
{
    None = VK_CULL_MODE_NONE,
    Front = VK_CULL_MODE_FRONT_BIT,
    Back = VK_CULL_MODE_BACK_BIT,
};

enum class Filtering : u32
{
    Nearest = VK_FILTER_NEAREST,
    Linear = VK_FILTER_LINEAR,
};

enum class TextureAddressMode : u32
{
    Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

enum class CompareOp : u32
{
    Never = VK_COMPARE_OP_NEVER,
    Less = VK_COMPARE_OP_LESS,
    Equal = VK_COMPARE_OP_EQUAL,
    LessEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater = VK_COMPARE_OP_GREATER,
    NotEqual = VK_COMPARE_OP_NOT_EQUAL,
    GreaterEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always = VK_COMPARE_OP_ALWAYS,
};

union ColorClearValue
{
    ColorClearValue(){};
    ColorClearValue(f32 r, f32 g, f32 b, f32 a)
        : m_val_float({ r, g, b, a }){};
    ColorClearValue(u32 r, u32 g, u32 b, u32 a)
        : m_val_uint({ r, g, b, a }){};
    ColorClearValue(i32 r, i32 g, i32 b, i32 a)
        : m_val_int({ r, g, b, a }){};
    ColorClearValue(const glm::vec4 &val)
        : m_val_float(val){};
    ColorClearValue(const glm::uvec4 &val)
        : m_val_uint(val){};
    ColorClearValue(const glm::ivec4 &val)
        : m_val_int(val){};

    glm::vec4 m_val_float = {};
    glm::uvec4 m_val_uint;
    glm::ivec4 m_val_int;
};

struct DepthClearValue
{
    DepthClearValue() = default;
    DepthClearValue(float depth, u8 stencil)
        : m_depth(depth),
          m_stencil(stencil){};

    float m_depth = 0.0f;
    u8 m_stencil = 0;
};

enum class BlendFactor : u32
{

    Zero = VK_BLEND_FACTOR_ZERO,
    One = VK_BLEND_FACTOR_ONE,
    SrcColor = VK_BLEND_FACTOR_SRC_COLOR,
    InvSrcColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    SrcAlpha = VK_BLEND_FACTOR_SRC_ALPHA,
    InvSrcAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    DstAlpha = VK_BLEND_FACTOR_DST_ALPHA,
    InvDstAlpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    DstColor = VK_BLEND_FACTOR_DST_COLOR,
    InvDstColor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    SrcAlphaSat = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    ConstantColor = VK_BLEND_FACTOR_CONSTANT_COLOR,
    InvConstantColor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    Src1Color = VK_BLEND_FACTOR_SRC1_COLOR,
    InvSrc1Color = VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
    Src1Alpha = VK_BLEND_FACTOR_SRC1_ALPHA,
    InvSrc1Alpha = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
};

enum class BlendOp : u32
{
    Add = VK_BLEND_OP_ADD,
    Subtract = VK_BLEND_OP_SUBTRACT,
    ReverseSubtract = VK_BLEND_OP_REVERSE_SUBTRACT,
    Min = VK_BLEND_OP_MIN,
    Max = VK_BLEND_OP_MAX,
};

enum class StencilOp : u32
{
    Keep = VK_STENCIL_OP_KEEP,
    Zero = VK_STENCIL_OP_ZERO,
    Replace = VK_STENCIL_OP_REPLACE,
    IncrAndClamp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    DecrAndClamp = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    Invert = VK_STENCIL_OP_INVERT,
    IncrAndWrap = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    DecrAndWrap = VK_STENCIL_OP_DECREMENT_AND_WRAP,
};

enum class ColorMask : u32
{
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,

    RGBA = R | G | B | A,
};
LR_TYPEOP_ARITHMETIC(ColorMask, ColorMask, |);

enum class DynamicState : u32
{
    Viewport = 1 << 0,          // VK_DYNAMIC_STATE_VIEWPORT,
    Scissor = 1 << 1,           // VK_DYNAMIC_STATE_SCISSOR,
    ViewportAndCount = 1 << 2,  // VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    ScissorAndCount = 1 << 3,   // VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    DepthTestEnable = 1 << 4,   // VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    DepthWriteEnable = 1 << 5,  // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    LineWidth = 1 << 6,         // VK_DYNAMIC_STATE_LINE_WIDTH,
    DepthBias = 1 << 7,         // VK_DYNAMIC_STATE_DEPTH_BIAS,
    BlendConstants = 1 << 8,    // VK_DYNAMIC_STATE_BLEND_CONSTANTS,

    VK_Viewport = VK_DYNAMIC_STATE_VIEWPORT,
    VK_Scissor = VK_DYNAMIC_STATE_SCISSOR,
    VK_ViewportAndCount = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    VK_ScissorAndCount = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    VK_DepthTestEnable = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    VK_DepthWriteEnable = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    VK_LineWidth = VK_DYNAMIC_STATE_LINE_WIDTH,
    VK_DepthBias = VK_DYNAMIC_STATE_DEPTH_BIAS,
    VK_BlendConstants = VK_DYNAMIC_STATE_BLEND_CONSTANTS,

    // DepthBounds = VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    // StencilCompareMask = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    // StencilWriteMask = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    // StencilReference = VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    // CullMode = VK_DYNAMIC_STATE_CULL_MODE,
    // FrontFace = VK_DYNAMIC_STATE_FRONT_FACE,
    // PrimitiveType = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    // InputBindingStride = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
    // DepthCompareOp = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    // DepthBoundsTestEnable = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
    // StencilTestEnable = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
    // StencilOp = VK_DYNAMIC_STATE_STENCIL_OP,
    // RasterDiscardEnable = VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
    // DepthBiasEnable = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
    // PrimitiveRestartEnable = VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
};
LR_TYPEOP_ARITHMETIC(DynamicState, DynamicState, |);
LR_TYPEOP_ARITHMETIC_INT(DynamicState, DynamicState, &);

enum class ShaderStage : u32
{
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Pixel = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
    TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    All = Vertex | Pixel | Compute | TessellationControl | TessellationEvaluation,
    Count = 5,
};
LR_TYPEOP_ARITHMETIC_INT(ShaderStage, ShaderStage, &);

/// MEMORY ---------------------------- ///
enum class MemoryFlag : u32
{
    Device = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    HostCached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,

    HostVisibleCoherent = HostVisible | HostCoherent,
};
LR_TYPEOP_ARITHMETIC_INT(MemoryFlag, MemoryFlag, &);
LR_TYPEOP_ARITHMETIC(MemoryFlag, MemoryFlag, |);

namespace Limits
{
    constexpr usize MaxPushConstants = 8;
    constexpr usize MaxVertexAttribs = 8;
    constexpr usize MaxColorAttachments = 8;
    constexpr usize MaxFrameCount = 8;

}  // namespace Limits
}  // namespace lr::Graphics
