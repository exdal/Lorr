#pragma once

#include "lorr.slang"  // IWYU pragma: export
#include "Vulkan.hh"   // IWYU pragma: export
#include "zfwd.hh"

#include <source_location>

namespace lr::graphics {
enum class DeviceFeature : u64 {
    None = 0,
    DescriptorBuffer = 1 << 0,
    MemoryBudget = 1 << 1,
};
LR_TYPEOP_ARITHMETIC_INT(DeviceFeature, DeviceFeature, &);
LR_TYPEOP_ARITHMETIC(DeviceFeature, DeviceFeature, |);
LR_TYPEOP_ASSIGNMENT(DeviceFeature, DeviceFeature, |);

enum class VKResult : i32 {
    Success = VK_SUCCESS,
    NotReady = VK_NOT_READY,
    TimeOut = VK_TIMEOUT,
    EventSet = VK_EVENT_SET,
    EventReset = VK_EVENT_RESET,
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
    OutOfPoolMem = VK_ERROR_OUT_OF_POOL_MEMORY,
    InvalidExternalHandle = VK_ERROR_INVALID_EXTERNAL_HANDLE,
    Fragmentation = VK_ERROR_FRAGMENTATION,
    InvalidOpaqueCaptureAddress = VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,
    PipelineCompileRequired = VK_PIPELINE_COMPILE_REQUIRED,
    SurfaceLost = VK_ERROR_SURFACE_LOST_KHR,
    WindowInUse = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
    Suboptimal = VK_SUBOPTIMAL_KHR,
    OutOfDate = VK_ERROR_OUT_OF_DATE_KHR,
    IncompatibleDisplay = VK_ERROR_INCOMPATIBLE_DISPLAY_KHR,
    ValidationFailed_EXT = VK_ERROR_VALIDATION_FAILED_EXT,
    InvalidShader_NV = VK_ERROR_INVALID_SHADER_NV,
    ImageUsageNotSupported = VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR,
    VideoPictureLayoutNotSupported = VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR,
    VideoProfileOperationNotSupported = VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR,
    VideoProfileCodecNotSupported = VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR,
    VideoSTDVersionNotSupported = VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR,
    InvalidDRMFormatModifierPlaneLayout_EXT = VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT,
    NotPermitted = VK_ERROR_NOT_PERMITTED_KHR,
    FullScreenExclusiveLost_EXT = VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT,
    ThreadIdle = VK_THREAD_IDLE_KHR,
    ThreadDone = VK_THREAD_DONE_KHR,
    OperationDeferred = VK_OPERATION_DEFERRED_KHR,
    OperationNotDeferred = VK_OPERATION_NOT_DEFERRED_KHR,
#ifdef VK_ENABLE_BETA_EXTENSONS
    InvalidVideoSTDParams = VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_EXT,
#endif
    CompressionExhausted_EXT = VK_ERROR_COMPRESSION_EXHAUSTED_EXT,
    IncompatibleShaderBinary_EXT = VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT,

    ResultMax = VK_RESULT_MAX_ENUM,
};

constexpr bool operator!(VKResult r)
{
    return r != VKResult::Success;
}

constexpr static std::string_view vkresult_to_string(VKResult result)
{
#define CASE(x)       \
    case VKResult::x: \
        return #x;

    switch (result) {
        CASE(Success);
        CASE(NotReady);
        CASE(TimeOut);
        CASE(Incomplete);
        CASE(OutOfHostMem);
        CASE(OutOfDeviceMem);
        CASE(InitFailed);
        CASE(DeviceLost);
        CASE(MemMapFailed);
        CASE(LayerNotPresent);
        CASE(ExtNotPresent);
        CASE(FeatureNotPresent);
        CASE(IncompatibleDriver);
        CASE(TooManyObjects);
        CASE(FormatNotSupported);
        CASE(FragmentedPool);
        CASE(Unknown);
        CASE(Fragmentation);
        CASE(SurfaceLost);
        CASE(WindowInUse);
        CASE(Suboptimal);
        CASE(OutOfDate);
        CASE(OutOfPoolMem);
        CASE(InvalidExternalHandle);
        CASE(InvalidOpaqueCaptureAddress);
        CASE(PipelineCompileRequired);
        CASE(IncompatibleDisplay);
        CASE(ValidationFailed_EXT);
        CASE(InvalidShader_NV);
        CASE(ImageUsageNotSupported);
        CASE(VideoPictureLayoutNotSupported);
        CASE(VideoProfileOperationNotSupported);
        CASE(VideoProfileCodecNotSupported);
        CASE(VideoSTDVersionNotSupported);
        CASE(InvalidDRMFormatModifierPlaneLayout_EXT);
        CASE(NotPermitted);
        CASE(FullScreenExclusiveLost_EXT);
        CASE(ThreadIdle);
        CASE(ThreadDone);
        CASE(OperationDeferred);
        CASE(OperationNotDeferred);
#ifdef VK_ENABLE_BETA_EXTENSONS
        CASE(InvalidVideoSTDParams);
#endif
        CASE(CompressionExhausted_EXT);
        CASE(IncompatibleShaderBinary_EXT);
        CASE(ResultMax);
        default:
            return "Unknown Error";
    }

#undef CASE
}

constexpr static VKResult CHECK(
    VkResult vkr, [[maybe_unused]] std::initializer_list<VKResult> allowed_checks = {}, [[maybe_unused]] std::source_location LOC = std::source_location::current())
{
    auto result = static_cast<VKResult>(vkr);
#if LR_DEBUG
    if (result != VKResult::Success)
        for (auto &a : allowed_checks)
            if (a == result)
                return result;

    LR_ASSERT(result == VKResult::Success, "Vulkan check failed at {}:{} - {}", LOC.file_name(), LOC.line(), LOC.function_name());
#endif

    return result;
}

enum class PresentMode : u32 {
    Immediate = VK_PRESENT_MODE_IMMEDIATE_KHR,
    Mailbox = VK_PRESENT_MODE_MAILBOX_KHR,
    Fifo = VK_PRESENT_MODE_FIFO_KHR,
    FifoRelazed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};

/// BUFFER ---------------------------- ///

enum class BufferUsage : u64 {
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

enum class CommandType : u32 {
    Graphics = 0,
    Compute,
    Transfer,
    Count,
};

enum class CommandTypeMask : u32 {
    Graphics = 1 << static_cast<u32>(CommandType::Graphics),
    Compute = 1 << static_cast<u32>(CommandType::Compute),
    Transfer = 1 << static_cast<u32>(CommandType::Transfer),
};
LR_TYPEOP_ARITHMETIC(CommandTypeMask, CommandTypeMask, |);
LR_TYPEOP_ARITHMETIC_INT(CommandTypeMask, CommandTypeMask, &);
LR_TYPEOP_ASSIGNMENT(CommandTypeMask, CommandTypeMask, |);

enum class CommandAllocatorFlag : u32 {
    None = 0,
    Transient = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    ResetCommandBuffer = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    Protected = VK_COMMAND_POOL_CREATE_PROTECTED_BIT,
};
LR_TYPEOP_ARITHMETIC(CommandAllocatorFlag, CommandAllocatorFlag, |);
LR_TYPEOP_ARITHMETIC_INT(CommandAllocatorFlag, CommandAllocatorFlag, &);

/// DESCRIPTOR ---------------------------- ///

enum class DescriptorType : u32 {
    Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
    SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,    // Read only image
    UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // Read only buffer
    StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,    // RW image
    StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RW Buffer

    Count = 5,
};

enum class DescriptorSetLayoutFlag : u32 {
    None = 0,
    UpdateAfterBindPool = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    DescriptorBuffer = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
    EmbeddedSamplers = VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT,
};
LR_TYPEOP_ARITHMETIC(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, &);

enum class DescriptorBindingFlag : u32 {
    None = 0,
    UpdateAfterBind = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
    UpdateUnusedWhilePending = VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
    PartiallyBound = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VariableDescriptorCount = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
};
LR_TYPEOP_ARITHMETIC(DescriptorBindingFlag, DescriptorBindingFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorBindingFlag, DescriptorBindingFlag, &);

enum class DescriptorPoolFlag : u32 {
    None = 0,
    // This flag allows individual frees for Descriptor Sets.
    // However comes with a cost, do not use it if not really needed.
    FreeDescriptorSet = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    UpdateAfterBind = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
};
LR_TYPEOP_ARITHMETIC(DescriptorPoolFlag, DescriptorPoolFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorPoolFlag, DescriptorPoolFlag, &);

/// IMAGE ---------------------------- ///

enum class Format : u32 {
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
    switch (format) {
        case Format::R32_SFLOAT:
        case Format::R32_SINT:
        case Format::R32_UINT:
            return sizeof(u32);
        case Format::R32G32_SFLOAT:
        case Format::R32G32_SINT:
        case Format::R32G32_UINT:
            return sizeof(u32) * 2;
        case Format::R32G32B32_SFLOAT:
        case Format::R32G32B32_SINT:
        case Format::R32G32B32_UINT:
            return sizeof(u32) * 3;
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_INT:
        case Format::R8G8B8A8_UINT:
        case Format::R8G8B8A8_SRGB:
            return sizeof(u8) * 4;
        case Format::R16G16B16A16_SFLOAT:
        case Format::R16G16B16A16_SINT:
        case Format::R16G16B16A16_UINT:
            return sizeof(u16) * 4;
        case Format::R32G32B32A32_SFLOAT:
        case Format::R32G32B32A32_SINT:
        case Format::R32G32B32A32_UINT:
            return sizeof(u32) * 4;
        case Format::B8G8R8A8_UNORM:
        case Format::B8G8R8A8_INT:
        case Format::B8G8R8A8_UINT:
        case Format::B8G8R8A8_SRGB:
            return sizeof(u8) * 4;
        case Format::D32_SFLOAT:
        case Format::D24_SFLOAT_S8_UINT:
            return sizeof(u32);
        default:
            break;
    }

    return 0;
}

enum class ImageUsage : u32 {
    Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    Storage = VK_IMAGE_USAGE_STORAGE_BIT,
};
LR_TYPEOP_ARITHMETIC(ImageUsage, ImageUsage, |);
LR_TYPEOP_ARITHMETIC_INT(ImageUsage, ImageUsage, &);

enum class ImageLayout : u32 {
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

enum class ImageType : u32 {
    View1D = VK_IMAGE_TYPE_1D,
    View2D = VK_IMAGE_TYPE_2D,
    View3D = VK_IMAGE_TYPE_3D,
};

enum class ImageViewType : u32 {
    View1D = VK_IMAGE_VIEW_TYPE_1D,
    View2D = VK_IMAGE_VIEW_TYPE_2D,
    View3D = VK_IMAGE_VIEW_TYPE_3D,
    Cube = VK_IMAGE_VIEW_TYPE_CUBE,
    Array1D = VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    Array2D = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    CubeArray = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
};

enum class ImageAspect : u32 {
    Color = VK_IMAGE_ASPECT_COLOR_BIT,
    Depth = VK_IMAGE_ASPECT_DEPTH_BIT,
    Stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
    DepthStencil = Depth | Stencil,
};

enum class ImageComponentSwizzle : u32 {
    Identity = VK_COMPONENT_SWIZZLE_IDENTITY,
    Zero = VK_COMPONENT_SWIZZLE_ZERO,
    One = VK_COMPONENT_SWIZZLE_ONE,
    R = VK_COMPONENT_SWIZZLE_R,
    G = VK_COMPONENT_SWIZZLE_G,
    B = VK_COMPONENT_SWIZZLE_B,
    A = VK_COMPONENT_SWIZZLE_A,
};

/// PIPELINE ---------------------------- ///

enum class PipelineBindPoint : u32 {
    Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    Compute = VK_PIPELINE_BIND_POINT_COMPUTE,
};

enum class PipelineStage : u64 {
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

enum class MemoryAccess : u64 {
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

struct PipelineAccessImpl {
    constexpr PipelineAccessImpl(MemoryAccess mem_access, PipelineStage pipeline_stage)
        : access(mem_access),
          stage(pipeline_stage) {};

    MemoryAccess access = MemoryAccess::None;
    PipelineStage stage = PipelineStage::None;

    auto operator<=>(const PipelineAccessImpl &) const = default;
};

constexpr PipelineAccessImpl operator|(const PipelineAccessImpl &lhs, const PipelineAccessImpl &rhs)
{
    return { lhs.access | rhs.access, lhs.stage | rhs.stage };
}

constexpr PipelineAccessImpl operator|=(PipelineAccessImpl &lhs, const PipelineAccessImpl &rhs)
{
    PipelineAccessImpl lhsn = { lhs.access | rhs.access, lhs.stage | rhs.stage };
    return lhs = lhsn | rhs;
}

namespace PipelineAccess {
#define LRX(name, access, stage) constexpr static PipelineAccessImpl name(access, stage)
    LRX(None, MemoryAccess::None, PipelineStage::None);
    LRX(TopOfPipe, MemoryAccess::None, PipelineStage::TopOfPipe);
    LRX(BottomOfPipe, MemoryAccess::None, PipelineStage::BottomOfPipe);
    LRX(VertexAttrib, MemoryAccess::Read, PipelineStage::VertexAttribInput);
    LRX(IndexAttrib, MemoryAccess::Read, PipelineStage::IndexAttribInput);

    LRX(IndirectRead, MemoryAccess::Read, PipelineStage::DrawIndirect);
    LRX(VertexShaderRead, MemoryAccess::Read, PipelineStage::VertexShader);
    LRX(TessControlRead, MemoryAccess::Read, PipelineStage::TessellationControl);
    LRX(TessEvalRead, MemoryAccess::Read, PipelineStage::TessellationEvaluation);
    LRX(PixelShaderRead, MemoryAccess::Read, PipelineStage::PixelShader);
    LRX(EarlyPixelTestsRead, MemoryAccess::Read, PipelineStage::EarlyPixelTests);
    LRX(LatePixelTestsRead, MemoryAccess::Read, PipelineStage::LatePixelTests);
    LRX(ColorAttachmentRead, MemoryAccess::Read, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsRead, MemoryAccess::Read, PipelineStage::AllGraphics);
    LRX(ComputeRead, MemoryAccess::Read, PipelineStage::ComputeShader);
    LRX(TransferRead, MemoryAccess::Read, PipelineStage::AllTransfer);
    LRX(HostRead, MemoryAccess::Read, PipelineStage::Host);

    LRX(IndirectWrite, MemoryAccess::Write, PipelineStage::DrawIndirect);
    LRX(VertexShaderWrite, MemoryAccess::Write, PipelineStage::VertexShader);
    LRX(TessControlWrite, MemoryAccess::Write, PipelineStage::TessellationControl);
    LRX(TessEvalWrite, MemoryAccess::Write, PipelineStage::TessellationEvaluation);
    LRX(PixelShaderWrite, MemoryAccess::Write, PipelineStage::PixelShader);
    LRX(EarlyPixelTestsWrite, MemoryAccess::Write, PipelineStage::EarlyPixelTests);
    LRX(LatePixelTestsWrite, MemoryAccess::Write, PipelineStage::LatePixelTests);
    LRX(ColorAttachmentWrite, MemoryAccess::Write, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsWrite, MemoryAccess::Write, PipelineStage::AllGraphics);
    LRX(ComputeWrite, MemoryAccess::Write, PipelineStage::ComputeShader);
    LRX(TransferWrite, MemoryAccess::Write, PipelineStage::AllTransfer);
    LRX(HostWrite, MemoryAccess::Write, PipelineStage::Host);

    LRX(IndirectReadWrite, MemoryAccess::ReadWrite, PipelineStage::DrawIndirect);
    LRX(VertexShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::VertexShader);
    LRX(TessControlReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationControl);
    LRX(TessEvalReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationEvaluation);
    LRX(PixelShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::PixelShader);
    LRX(EarlyPixelTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::EarlyPixelTests);
    LRX(LatePixelTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::LatePixelTests);
    LRX(ColorAttachmentReadWrite, MemoryAccess::ReadWrite, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllGraphics);
    LRX(ComputeReadWrite, MemoryAccess::ReadWrite, PipelineStage::ComputeShader);
    LRX(TransferReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllTransfer);
    LRX(HostReadWrite, MemoryAccess::ReadWrite, PipelineStage::Host);
#undef LRX
}  // namespace PipelineAccess

enum class PrimitiveType : u32 {
    PointList = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    LineList = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    TriangleList = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    Patch = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
};

enum class VertexInputRate {
    Vertex = 0,
    Instanced = 1,
};

enum class CullMode : u32 {
    None = VK_CULL_MODE_NONE,
    Front = VK_CULL_MODE_FRONT_BIT,
    Back = VK_CULL_MODE_BACK_BIT,
};

enum class Filtering : u32 {
    Nearest = VK_FILTER_NEAREST,
    Linear = VK_FILTER_LINEAR,
};

enum class TextureAddressMode : u32 {
    Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

enum class CompareOp : u32 {
    Never = VK_COMPARE_OP_NEVER,
    Less = VK_COMPARE_OP_LESS,
    Equal = VK_COMPARE_OP_EQUAL,
    LessEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater = VK_COMPARE_OP_GREATER,
    NotEqual = VK_COMPARE_OP_NOT_EQUAL,
    GreaterEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always = VK_COMPARE_OP_ALWAYS,
};

enum class AttachmentLoadOp : u32 {
    Load = VK_ATTACHMENT_LOAD_OP_LOAD,
    Clear = VK_ATTACHMENT_LOAD_OP_CLEAR,
    DontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
};

enum class AttachmentStoreOp : u32 {
    Store = VK_ATTACHMENT_STORE_OP_STORE,
    DontCare = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    None = VK_ATTACHMENT_STORE_OP_NONE,
};

union ColorClearValue {
    ColorClearValue() {};
    ColorClearValue(f32 r, f32 g, f32 b, f32 a)
        : m_val_float({ r, g, b, a }) {};
    ColorClearValue(u32 r, u32 g, u32 b, u32 a)
        : m_val_uint({ r, g, b, a }) {};
    ColorClearValue(i32 r, i32 g, i32 b, i32 a)
        : m_val_int({ r, g, b, a }) {};
    ColorClearValue(const glm::vec4 &val)
        : m_val_float(val) {};
    ColorClearValue(const glm::uvec4 &val)
        : m_val_uint(val) {};
    ColorClearValue(const glm::ivec4 &val)
        : m_val_int(val) {};

    glm::vec4 m_val_float = {};
    glm::uvec4 m_val_uint;
    glm::ivec4 m_val_int;
};

struct DepthClearValue {
    DepthClearValue() = default;
    DepthClearValue(float depth, u8 stencil)
        : m_depth(depth),
          m_stencil(stencil) {};

    float m_depth = 0.0f;
    u8 m_stencil = 0;
};

enum class BlendFactor : u32 {
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

enum class BlendOp : u32 {
    Add = VK_BLEND_OP_ADD,
    Subtract = VK_BLEND_OP_SUBTRACT,
    ReverseSubtract = VK_BLEND_OP_REVERSE_SUBTRACT,
    Min = VK_BLEND_OP_MIN,
    Max = VK_BLEND_OP_MAX,
};

enum class StencilOp : u32 {
    Keep = VK_STENCIL_OP_KEEP,
    Zero = VK_STENCIL_OP_ZERO,
    Replace = VK_STENCIL_OP_REPLACE,
    IncrAndClamp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    DecrAndClamp = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    Invert = VK_STENCIL_OP_INVERT,
    IncrAndWrap = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    DecrAndWrap = VK_STENCIL_OP_DECREMENT_AND_WRAP,
};

enum class ColorMask : u32 {
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,

    RGBA = R | G | B | A,
};
LR_TYPEOP_ARITHMETIC(ColorMask, ColorMask, |);

enum class DynamicState : u32 {
    Viewport = 1 << 0,                 // VK_DYNAMIC_STATE_VIEWPORT,
    Scissor = 1 << 1,                  // VK_DYNAMIC_STATE_SCISSOR,
    LineWidth = 1 << 2,                // VK_DYNAMIC_STATE_LINE_WIDTH,
    DepthBias = 1 << 3,                // VK_DYNAMIC_STATE_DEPTH_BIAS,
    BlendConstants = 1 << 4,           // VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    DepthBounds = 1 << 5,              // VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    StencilCompareMask = 1 << 6,       // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    StencilWriteMask = 1 << 7,         // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    StencilReference = 1 << 8,         // VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    CullMode = 1 << 9,                 // VK_DYNAMIC_STATE_CULL_MODE,
    FrontFace = 1 << 10,               // VK_DYNAMIC_STATE_FRONT_FACE,
    PrimitiveType = 1 << 11,           // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    ViewportAndCount = 1 << 12,        // VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    ScissorAndCount = 1 << 13,         // VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    InputBindingStride = 1 << 14,      // VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
    DepthTestEnable = 1 << 15,         // VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    DepthWriteEnable = 1 << 16,        // VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    DepthCompareOp = 1 << 17,          // VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    DepthBoundsTestEnable = 1 << 18,   // VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
    StencilTestEnable = 1 << 19,       // VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
    StencilOp = 1 << 20,               // VK_DYNAMIC_STATE_STENCIL_OP,
    RasterDiscardEnable = 1 << 21,     // VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
    DepthBiasEnable = 1 << 22,         // VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
    PrimitiveRestartEnable = 1 << 23,  // VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,

    // YOU MUST UPDATE THESE!!!
    Last = PrimitiveRestartEnable,
    Count = 24,  // Always Last + 1 because bitwise
};
LR_TYPEOP_ARITHMETIC(DynamicState, DynamicState, |);
LR_TYPEOP_ARITHMETIC_INT(DynamicState, DynamicState, &);

enum class ShaderStageFlag : u32 {
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Pixel = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
    TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    All = Vertex | Pixel | Compute | TessellationControl | TessellationEvaluation,
    Count = 5,
};
LR_TYPEOP_ARITHMETIC_INT(ShaderStageFlag, ShaderStageFlag, &);

/// MEMORY ---------------------------- ///
enum class MemoryFlag : u32 {
    None = 0,
    Dedicated = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,  //< Require dedicated MEMORY POOL.
    HostSeqWrite = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    HostReadWrite = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
};
LR_TYPEOP_ARITHMETIC(MemoryFlag, MemoryFlag, |);
LR_TYPEOP_ARITHMETIC_INT(MemoryFlag, MemoryFlag, &);

enum class MemoryPreference : u32 {
    Auto = VMA_MEMORY_USAGE_AUTO,
    Host = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    Device = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
};

namespace Limits {
    constexpr usize FrameCount = 3;
}  // namespace Limits

// Vulkan Structs
struct Offset2D {
    i32 x = 0;
    i32 y = 0;

    bool operator==(const Offset2D &) const = default;
    bool operator!=(const Offset2D &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkOffset2D *>(this); }
    operator auto &() { return *reinterpret_cast<VkOffset2D *>(this); }
};

struct Offset3D {
    i32 x = 0;
    i32 y = 0;
    i32 z = 0;

    bool operator==(const Offset3D &) const = default;
    bool operator!=(const Offset3D &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkOffset3D *>(this); }
    operator auto &() { return *reinterpret_cast<VkOffset3D *>(this); }
};

struct Extent2D {
    u32 width = 0;
    u32 height = 0;

    bool operator==(const Extent2D &) const = default;
    bool operator!=(const Extent2D &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkExtent2D *>(this); }
    operator auto &() { return *reinterpret_cast<VkExtent2D *>(this); }
};

struct Extent3D {
    u32 width = 0;
    u32 height = 0;
    u32 depth = 0;

    bool operator==(const Extent3D &) const = default;
    bool operator!=(const Extent3D &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkExtent3D *>(this); }
    operator auto &() { return *reinterpret_cast<VkExtent3D *>(this); }
};

struct Viewport {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
    float depth_min = 0.0f;
    float depth_max = 1.0f;

    bool operator==(const Viewport &) const = default;
    bool operator!=(const Viewport &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkViewport *>(this); }
    operator auto &() { return *reinterpret_cast<VkViewport *>(this); }
};

struct Rect2D {
    Offset2D offset = {};
    Extent2D extent = {};

    bool operator==(const Rect2D &) const = default;
    bool operator!=(const Rect2D &) const = default;
    operator auto const &() const { return *reinterpret_cast<const VkRect2D *>(this); }
    operator auto &() { return *reinterpret_cast<VkRect2D *>(this); }
};

// Subresource range, it's in the name
struct ImageSubresourceRange {
    using VkType = VkImageSubresourceRange;

    ImageAspect aspect_mask = ImageAspect::Color;
    u32 base_mip = 0;
    u32 mip_count = 1;
    u32 base_slice = 0;
    u32 slice_count = 1;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(ImageSubresourceRange::VkType) == sizeof(ImageSubresourceRange));

// Subresource info used for layers(slices)
struct ImageSubresourceLayers {
    using VkType = VkImageSubresourceLayers;

    ImageAspect aspect_mask = ImageAspect::Color;
    u32 target_mip = 0;
    u32 base_slice = 0;
    u32 slice_count = 1;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(ImageSubresourceLayers::VkType) == sizeof(ImageSubresourceLayers));

struct PushConstantRange {
    using VkType = VkPushConstantRange;

    ShaderStageFlag stage = ShaderStageFlag::All;
    u32 offset = 0;
    u32 size = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(PushConstantRange::VkType) == sizeof(PushConstantRange));

struct PipelineShaderStageInfo {
    using VkType = VkPipelineShaderStageCreateInfo;

    VkStructureType struct_type = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    void *next = nullptr;
    VkPipelineShaderStageCreateFlags create_flags = {};
    ShaderStageFlag shader_stage = ShaderStageFlag::All;
    VkShaderModule module = VK_NULL_HANDLE;
    const char *entry_point = nullptr;
    VkSpecializationInfo *specialization_info = nullptr;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(PipelineShaderStageInfo::VkType) == sizeof(PipelineShaderStageInfo));

struct PipelineViewportStateInfo {
    using VkType = VkPipelineViewportStateCreateInfo;

    VkStructureType struct_type = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    const void *next = nullptr;
    VkPipelineViewportStateCreateFlags create_flags = 0;
    u32 viewport_count = 0;
    Viewport *viewports = nullptr;
    u32 scissor_count = 0;
    Rect2D *scissors = nullptr;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};

static_assert(sizeof(PipelineViewportStateInfo::VkType) == sizeof(PipelineViewportStateInfo));

struct StencilFaceOp {
    using VkType = VkStencilOpState;

    StencilOp pass = {};
    StencilOp fail = {};
    CompareOp depth_fail = {};
    CompareOp compare_func = {};
    ColorMask compare_mask = ColorMask::None;
    ColorMask write_mask = ColorMask::None;
    u32 reference = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(StencilFaceOp::VkType) == sizeof(StencilFaceOp));

struct PipelineColorBlendAttachment {
    using VkType = VkPipelineColorBlendAttachmentState;

    b32 blend_enabled = false;
    BlendFactor src_blend = BlendFactor::SrcAlpha;
    BlendFactor dst_blend = BlendFactor::InvSrcAlpha;
    BlendOp blend_op = BlendOp::Add;
    BlendFactor src_blend_alpha = BlendFactor::One;
    BlendFactor dst_blend_alpha = BlendFactor::InvSrcAlpha;
    BlendOp blend_op_alpha = BlendOp::Add;
    ColorMask write_mask = ColorMask::RGBA;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(PipelineColorBlendAttachment::VkType) == sizeof(PipelineColorBlendAttachment));

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

struct ImageBarrier {
    PipelineAccessImpl src_access = PipelineAccess::None;
    PipelineAccessImpl dst_access = PipelineAccess::None;
    ImageLayout old_layout = ImageLayout::Undefined;
    ImageLayout new_layout = ImageLayout::Undefined;
    u32 src_queue_family_id = ~0u;
    u32 dst_queue_family_id = ~0u;
    ImageID image_id = ImageID::Invalid;
    ImageSubresourceRange subresource_range = {};

    VkImageMemoryBarrier2 vk_type(VkImage image)
    {
        return { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                 .pNext = nullptr,
                 .srcStageMask = VkPipelineStageFlags2(src_access.stage),
                 .srcAccessMask = VkAccessFlags2(src_access.access),
                 .dstStageMask = VkPipelineStageFlags2(dst_access.stage),
                 .dstAccessMask = VkAccessFlags2(dst_access.access),
                 .oldLayout = VkImageLayout(old_layout),
                 .newLayout = VkImageLayout(new_layout),
                 .srcQueueFamilyIndex = src_queue_family_id,
                 .dstQueueFamilyIndex = dst_queue_family_id,
                 .image = image,
                 .subresourceRange = subresource_range };
    }
};

struct MemoryBarrier {
    PipelineAccessImpl src_access = PipelineAccess::None;
    PipelineAccessImpl dst_access = PipelineAccess::None;

    VkMemoryBarrier2 vk_type()
    {
        return { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                 .pNext = nullptr,
                 .srcStageMask = VkPipelineStageFlags2(src_access.stage),
                 .srcAccessMask = VkAccessFlags2(src_access.access),
                 .dstStageMask = VkPipelineStageFlags2(dst_access.stage),
                 .dstAccessMask = VkAccessFlags2(dst_access.access) };
    }
};

struct BufferCopyRegion {
    using VkType = VkBufferCopy;

    u64 src_offset = 0;
    u64 dst_offset = 0;
    u64 size = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(BufferCopyRegion::VkType) == sizeof(BufferCopyRegion));

struct ImageCopyRegion {
    using VkType = VkBufferImageCopy;

    u64 buffer_offset = 0;
    u64 buffer_row_length = 0;
    ImageSubresourceLayers image_subresource_layer = {};
    Offset3D image_offset = {};
    Extent3D image_extent = {};

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(ImageCopyRegion::VkType) == sizeof(ImageCopyRegion));

struct ImageBlit {
    using VkType = VkImageBlit2;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    void *next = nullptr;
    ImageSubresourceLayers src_subresource = {};
    Offset3D src_offsets[2] = {};
    ImageSubresourceLayers dst_subresource = {};
    Offset3D dst_offsets[2] = {};

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(ImageBlit::VkType) == sizeof(ImageBlit));

struct CommandListSubmitInfo {
    CommandListSubmitInfo() = default;
    CommandListSubmitInfo(VkCommandBuffer command_list_)
        : command_list(command_list_)
    {
    }

    using VkType = VkCommandBufferSubmitInfo;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    void *next = nullptr;
    VkCommandBuffer command_list = VK_NULL_HANDLE;
    // TODO: Multidevices
    u32 device_mask = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(CommandListSubmitInfo::VkType) == sizeof(CommandListSubmitInfo));

struct SemaphoreSubmitInfo {
    SemaphoreSubmitInfo() = default;
    SemaphoreSubmitInfo(VkSemaphore semaphore_, u64 value_, PipelineStage stages_)
        : semaphore(semaphore_),
          value(value_),
          stage_mask(stages_)
    {
    }

    using VkType = VkSemaphoreSubmitInfo;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    void *next = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    u64 value = 0;
    PipelineStage stage_mask = PipelineStage::None;
    u32 device_index = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(SemaphoreSubmitInfo::VkType) == sizeof(SemaphoreSubmitInfo));

struct QueueSubmitInfo {
    using VkType = VkSubmitInfo2;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    void *next = nullptr;
    VkSubmitFlags submit_flags = 0;

    u32 wait_sema_count = 0;
    SemaphoreSubmitInfo *wait_sema_infos = nullptr;

    u32 command_list_count = 0;
    CommandListSubmitInfo *command_list_infos = nullptr;

    u32 signal_sema_count = 0;
    SemaphoreSubmitInfo *signal_sema_infos = nullptr;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(QueueSubmitInfo::VkType) == sizeof(QueueSubmitInfo));

struct RenderingAttachmentInfo {
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout image_layout = ImageLayout::Undefined;
    AttachmentLoadOp load_op = AttachmentLoadOp::DontCare;
    AttachmentStoreOp store_op = AttachmentStoreOp::DontCare;
    union {
        ColorClearValue color_clear = {};
        DepthClearValue depth_clear;
    } clear_value;

    VkRenderingAttachmentInfo vk_info(VkImageView image_view)
    {
        return {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = image_view,
            .imageLayout = VkImageLayout(image_layout),
            .resolveMode = {},
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VkAttachmentLoadOp(load_op),
            .storeOp = VkAttachmentStoreOp(store_op),
            .clearValue = *reinterpret_cast<VkClearValue *>(&clear_value),
        };
    }
};

struct RenderingBeginInfo {
    Rect2D render_area = {};
    std::span<RenderingAttachmentInfo> color_attachments = {};
    RenderingAttachmentInfo *depth_attachment = nullptr;
    RenderingAttachmentInfo *stencil_attachment = nullptr;
};

struct DescriptorSetLayoutElement {
    using VkType = VkDescriptorSetLayoutBinding;

    u32 binding = ~0u;
    DescriptorType descriptor_type = DescriptorType::Count;
    u32 descriptor_count = 0;
    ShaderStageFlag stage = ShaderStageFlag::Count;
    VkSampler *immutable_samplers_unused = nullptr;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(DescriptorSetLayoutElement::VkType) == sizeof(DescriptorSetLayoutElement));

struct ImageDescriptorInfo {
    using VkType = VkDescriptorImageInfo;

    VkSampler sampler = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    ImageLayout image_layout = ImageLayout::Undefined;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(ImageDescriptorInfo::VkType) == sizeof(ImageDescriptorInfo));

struct BufferDescriptorInfo {
    using VkType = VkDescriptorBufferInfo;

    VkBuffer buffer = VK_NULL_HANDLE;
    u64 offset = 0;
    u64 range = ~0ULL;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(BufferDescriptorInfo::VkType) == sizeof(BufferDescriptorInfo));

struct WriteDescriptorSet {
    using VkType = VkWriteDescriptorSet;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    void *next = nullptr;
    VkDescriptorSet dst_descriptor_set = VK_NULL_HANDLE;
    u32 dst_binding = 0;
    u32 dst_element = 0;
    u32 count = 0;
    DescriptorType type = DescriptorType::Count;
    ImageDescriptorInfo *image_info = nullptr;
    BufferDescriptorInfo *buffer_info = nullptr;
    VkBufferView *texel_buffer_info_unused = nullptr;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(WriteDescriptorSet::VkType) == sizeof(WriteDescriptorSet));

struct CopyDescriptorSet {
    using VkType = VkCopyDescriptorSet;

    VkStructureType structure_type = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    void *next = nullptr;
    VkDescriptorSet src_descriptor_set = VK_NULL_HANDLE;
    u32 src_binding = 0;
    u32 src_element = 0;
    VkDescriptorSet dst_descriptor_set = VK_NULL_HANDLE;
    u32 dst_binding = 0;
    u32 dst_element = 0;
    u32 count = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(CopyDescriptorSet::VkType) == sizeof(CopyDescriptorSet));

struct DescriptorPoolSize {
    using VkType = VkDescriptorPoolSize;

    DescriptorType type = DescriptorType::Count;
    u32 count = 0;

    operator auto &() { return *reinterpret_cast<VkType *>(this); }
    operator const auto &() const { return *reinterpret_cast<const VkType *>(this); }
};
static_assert(sizeof(DescriptorPoolSize::VkType) == sizeof(DescriptorPoolSize));

template<typename T>
struct Unique {
    using this_type = Unique<T>;
    using val_type = T;

    explicit Unique() = default;
    explicit Unique(const Unique &) = delete;
    Unique(Unique &&other) noexcept
        : m_device(other.m_device),
          m_val(other.release())
    {
    }

    explicit Unique(Device *device)
        : m_device(device),
          m_val({})
    {
    }

    explicit Unique(Device *device, val_type val)
        : m_device(device),
          m_val(std::move(val))
    {
    }

    ~Unique() noexcept { reset(); }

    val_type &get() noexcept { return m_val; }
    const val_type &get() const noexcept { return m_val; }
    val_type release() noexcept
    {
        m_device = nullptr;
        return std::move(m_val);
    }

    void set_name(std::string_view name);
    void reset(val_type val = {}) noexcept;
    void swap(this_type &other) noexcept
    {
        std::swap(m_device, other.m_device);
        std::swap(m_val, other.m_val);
    }

    explicit operator bool() const noexcept { return m_val; }
    val_type *operator->() noexcept { return &m_val; }
    const val_type *operator->() const noexcept { return &m_val; }
    val_type &operator*() noexcept { return m_val; }
    const val_type &operator*() const noexcept { return m_val; }

    this_type &operator=(const this_type &) = delete;
    this_type &operator=(this_type &&other) noexcept
    {
        Device *temp = other.m_device;
        reset(other.release());
        m_device = temp;
        return *this;
    }

    this_type &operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    Device *m_device = nullptr;
    val_type m_val = {};
};

template<typename T>
constexpr void swap(Unique<T> &lhs, Unique<T> &rhs) noexcept
{
    lhs.swap(rhs);
}
}  // namespace lr::graphics

namespace fmt {
template<>
struct formatter<lr::graphics::VKResult> : formatter<string_view> {
    template<typename FormatContext>
    constexpr auto format(lr::graphics::VKResult v, FormatContext &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}({})", lr::graphics::vkresult_to_string(v), static_cast<i32>(v));
    }
};
}  // namespace fmt
