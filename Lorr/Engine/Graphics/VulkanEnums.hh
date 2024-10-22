#pragma once

namespace lr::vk {
enum class Result : i32 {
    Success = 0,
    NotReady,
    TimeOut,
    EventSet,
    EventReset,
    Incomplete,
    OutOfHostMem,
    OutOfDeviceMem,
    InitFailed,
    DeviceLost,
    MemMapFailed,
    LayerNotPresent,
    ExtNotPresent,
    FeatureNotPresent,
    IncompatibleDriver,
    TooManyObjects,
    FormatNotSupported,
    FragmentedPool,
    Unknown,
    OutOfPoolMem,
    InvalidExternalHandle,
    Fragmentation,
    InvalidOpaqueCaptureAddress,
    PipelineCompileRequired,
    SurfaceLost,
    WindowInUse,
    Suboptimal,
    OutOfDate,
    IncompatibleDisplay,
    ValidationFailed_EXT,
    ImageUsageNotSupported,
    VideoPictureLayoutNotSupported,
    VideoProfileOperationNotSupported,
    VideoProfileCodecNotSupported,
    VideoSTDVersionNotSupported,
    NotPermitted,
    ThreadIdle,
    ThreadDone,
    OperationDeferred,
    OperationNotDeferred,

    Count,
};

enum class Format : u32 {
    // Format: CB_T
    // C = Component, B = bit count, T = type

    // Generic types -- component dependant types
    // Because of autism, enums are sorted by component size
    // so i can finally stop thinking of this over and over again
    Undefined = 0,

    // 1 byte
    R8_UNORM,

    // 2 byte

    // 4 byte
    R8G8B8A8_UNORM,
    R8G8B8A8_SINT,
    R8G8B8A8_UINT,
    R8G8B8A8_SRGB,

    B8G8R8A8_UNORM,
    B8G8R8A8_SINT,
    B8G8R8A8_UINT,
    B8G8R8A8_SRGB,

    R32_SFLOAT,
    R32_SINT,
    R32_UINT,

    D32_SFLOAT,
    D24_UNORM_S8_UINT,

    // 8 byte
    R16G16B16A16_SFLOAT,
    R16G16B16A16_SINT,
    R16G16B16A16_UINT,

    R32G32_SFLOAT,
    R32G32_SINT,
    R32G32_UINT,

    // 12 byte
    R32G32B32_SFLOAT,
    R32G32B32_SINT,
    R32G32B32_UINT,

    // 16 byte
    R32G32B32A32_SFLOAT,
    R32G32B32A32_SINT,
    R32G32B32A32_UINT,
};

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

consteval void enable_bitmask(CommandTypeMask);

enum class CommandAllocatorFlag : u32 {
    None = 0,
    Transient,
    ResetCommandBuffer,
    Protected,
};
consteval void enable_bitmask(CommandAllocatorFlag);

enum class BufferUsage : u64 {
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    TransferSrc = 1 << 2,
    TransferDst = 1 << 3,
    Storage = 1 << 4,
};
consteval void enable_bitmask(BufferUsage);

enum class ImageUsage : u32 {
    None = 0,
    Sampled = 1 << 0,
    ColorAttachment = 1 << 1,
    DepthStencilAttachment = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4,
    Storage = 1 << 5,
};
consteval void enable_bitmask(ImageUsage);

enum class ImageLayout : u32 {
    Undefined = 0,
    General = 1,
    ColorAttachment = 2,
    DepthStencilAttachment = 3,
    DepthStencilReadOnly = 4,
    ShaderReadOnly = 5,
    TransferSrc = 6,
    TransferDst = 7,
    Present = 1000001002,
    DepthAttachment = 1000241000,
    DepthReadOnly = 1000241001,
    StencilAttachment = 1000241002,
    StencilReadOnly = 1000241003,
    ColorReadOnly = 1000314000,
};

enum class ImageType : u32 {
    View1D = 0,
    View2D,
    View3D,
};

enum class ImageViewType : u32 {
    View1D = 0,
    View2D,
    View3D,
    Cube,
    Array1D,
    Array2D,
    CubeArray,
};

enum class ImageAspectFlag : u32 {
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
};
consteval void enable_bitmask(ImageAspectFlag);

// Safe to cast VK type
enum class PipelineBindPoint : u32 {
    Graphics = 0,
    Compute,
};

// PERF: This is performance critical enum, so I had to
// manually assign VK flags to it
//
// Safe to cast VK type
enum class PipelineStage : u64 {
    None = 0,
    TopOfPipe = 1 << 0,

    DrawIndirect = 1 << 1,

    IndexAttribInput = 0x1000000000ULL,
    VertexAttribInput = 0x2000000000ULL,
    VertexShader = 1 << 3,
    TessellationControl = 1 << 4,
    TessellationEvaluation = 1 << 5,
    FragmentShader = 1 << 7,
    EarlyFragmentTests = 1 << 8,
    LateFragmentTests = 1 << 9,
    DepthStencilTests = EarlyFragmentTests | LateFragmentTests,
    ColorAttachmentOutput = 1 << 10,

    ComputeShader = 1 << 11,
    AllTransfer = 1 << 12,
    BottomOfPipe = 1 << 13,
    Host = 1 << 14,
    AllGraphics = 1 << 15,
    AllCommands = 1 << 16,

    Copy = 1_u64 << 32,
    Resolve = 1_u64 << 33,
    Blit = 1_u64 << 34,
    Clear = 1_u64 << 35,
};
consteval void enable_bitmask(PipelineStage);

// Safe to cast VK type
enum class MemoryAccess : u64 {
    None = 0,
    IndirectRead = 1 << 0,
    IndexAttribRead = 1 << 1,
    VertexAttribRead = 1 << 2,
    UniformRead = 1 << 3,
    InputAttachmentRead = 1 << 4,
    ShaderRead = 1 << 5,
    ShaderWrite = 1 << 6,
    ColorAttachmentRead = 1 << 7,
    ColorAttachmentWrite = 1 << 8,
    DepthStencilAttachmentRead = 1 << 9,
    DepthStencilAttachmentWrite = 1 << 10,
    TransferRead = 1 << 11,
    TransferWrite = 1 << 12,
    HostRead = 1 << 13,
    HostWrite = 1 << 14,
    Read = 1 << 15,
    Write = 1 << 16,

    SampledRead = 1_u64 << 32,
    StorageRead = 1_u64 << 33,
    StorageWrite = 1_u64 << 34,
    ReadWrite = Read | Write,
};
consteval void enable_bitmask(MemoryAccess);

enum class PrimitiveType : u32 {
    PointList = 0,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    Patch,
};

enum class VertexInputRate : u32 {
    Vertex = 0,
    Instanced,
};

enum class CullMode : u32 {
    None = 0,
    Front,
    Back,
    FrontBack,
};

enum class Filtering : u32 {
    Nearest = 0,
    Linear,
};

enum class SamplerAddressMode : u32 {
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

enum class CompareOp : u32 {
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

// Safe to cast into VK type
enum class AttachmentLoadOp : u32 {
    Load = 0,
    Clear,
    DontCare,
};

// Safe to cast into VK type
enum class AttachmentStoreOp : u32 {
    Store = 0,
    DontCare,
    None = 1000301000,
};

enum class BlendFactor : u32 {
    Zero = 0,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    DstColor,
    InvDstColor,
    SrcAlphaSat,
    ConstantColor,
    InvConstantColor,
    Src1Color,
    InvSrc1Color,
    Src1Alpha,
    InvSrc1Alpha,
};

enum class BlendOp : u32 {
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class StencilOp : u32 {
    Keep = 0,
    Zero,
    Replace,
    IncrAndClamp,
    DecrAndClamp,
    Invert,
    IncrAndWrap,
    DecrAndWrap,
};

enum class ColorMask : u32 {
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,

    RGBA = R | G | B | A,
};
consteval void enable_bitmask(ColorMask);

enum class ShaderStageFlag : u32 {
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
    TessellationControl = 1 << 3,
    TessellationEvaluation = 1 << 4,
    All = Vertex | Fragment | Compute | TessellationControl | TessellationEvaluation,
    Count = 5,
};
consteval void enable_bitmask(ShaderStageFlag);

enum class MemoryAllocationFlag : u32 {
    None = 0,
    Dedicated = 1 << 0,  //< Require dedicated MEMORY POOL.
    HostSeqWrite = 1 << 1,
    HostReadWrite = 1 << 2,
};
constexpr void enable_bitmask(MemoryAllocationFlag);

enum class MemoryAllocationUsage : u32 {
    Auto = 0,
    PreferHost,
    PreferDevice,
};

using lr::operator|;
struct PipelineAccessImpl {
    constexpr PipelineAccessImpl(MemoryAccess mem_access, PipelineStage pipeline_stage)
        : access(mem_access),
          stage(pipeline_stage) {};

    MemoryAccess access = MemoryAccess::None;
    PipelineStage stage = PipelineStage::None;

    auto operator<=>(const PipelineAccessImpl &) const = default;
};

constexpr PipelineAccessImpl operator|(const PipelineAccessImpl &lhs, const PipelineAccessImpl &rhs) {
    return { lhs.access | rhs.access, lhs.stage | rhs.stage };
}

constexpr PipelineAccessImpl operator|=(PipelineAccessImpl &lhs, const PipelineAccessImpl &rhs) {
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
    LRX(All, MemoryAccess::ReadWrite, PipelineStage::AllCommands);

    LRX(IndirectRead, MemoryAccess::Read, PipelineStage::DrawIndirect);
    LRX(VertexShaderRead, MemoryAccess::Read, PipelineStage::VertexShader);
    LRX(TessControlRead, MemoryAccess::Read, PipelineStage::TessellationControl);
    LRX(TessEvalRead, MemoryAccess::Read, PipelineStage::TessellationEvaluation);
    LRX(FragmentShaderRead, MemoryAccess::Read, PipelineStage::FragmentShader);
    LRX(EarlyFragmentTestsRead, MemoryAccess::Read, PipelineStage::EarlyFragmentTests);
    LRX(LateFragmentTestsRead, MemoryAccess::Read, PipelineStage::LateFragmentTests);
    LRX(DepthStencilRead, MemoryAccess::Read, PipelineStage::DepthStencilTests);
    LRX(ColorAttachmentRead, MemoryAccess::Read, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsRead, MemoryAccess::Read, PipelineStage::AllGraphics);
    LRX(ComputeRead, MemoryAccess::Read, PipelineStage::ComputeShader);
    LRX(TransferRead, MemoryAccess::Read, PipelineStage::AllTransfer);
    LRX(BlitRead, MemoryAccess::Read, PipelineStage::Blit);
    LRX(HostRead, MemoryAccess::Read, PipelineStage::Host);

    LRX(IndirectWrite, MemoryAccess::Write, PipelineStage::DrawIndirect);
    LRX(VertexShaderWrite, MemoryAccess::Write, PipelineStage::VertexShader);
    LRX(TessControlWrite, MemoryAccess::Write, PipelineStage::TessellationControl);
    LRX(TessEvalWrite, MemoryAccess::Write, PipelineStage::TessellationEvaluation);
    LRX(FragmentShaderWrite, MemoryAccess::Write, PipelineStage::FragmentShader);
    LRX(EarlyFragmentTestsWrite, MemoryAccess::Write, PipelineStage::EarlyFragmentTests);
    LRX(LateFragmentTestsWrite, MemoryAccess::Write, PipelineStage::LateFragmentTests);
    LRX(DepthStencilWrite, MemoryAccess::Write, PipelineStage::DepthStencilTests);
    LRX(ColorAttachmentWrite, MemoryAccess::Write, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsWrite, MemoryAccess::Write, PipelineStage::AllGraphics);
    LRX(ComputeWrite, MemoryAccess::Write, PipelineStage::ComputeShader);
    LRX(BlitWrite, MemoryAccess::Write, PipelineStage::Blit);
    LRX(TransferWrite, MemoryAccess::Write, PipelineStage::AllTransfer);
    LRX(HostWrite, MemoryAccess::Write, PipelineStage::Host);

    LRX(IndirectReadWrite, MemoryAccess::ReadWrite, PipelineStage::DrawIndirect);
    LRX(VertexShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::VertexShader);
    LRX(TessControlReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationControl);
    LRX(TessEvalReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationEvaluation);
    LRX(FragmentShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::FragmentShader);
    LRX(EarlyFragmentTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::EarlyFragmentTests);
    LRX(LateFragmentTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::LateFragmentTests);
    LRX(DepthStencilReadWrite, MemoryAccess::ReadWrite, PipelineStage::DepthStencilTests);
    LRX(ColorAttachmentReadWrite, MemoryAccess::ReadWrite, PipelineStage::ColorAttachmentOutput);
    LRX(GraphicsReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllGraphics);
    LRX(ComputeReadWrite, MemoryAccess::ReadWrite, PipelineStage::ComputeShader);
    LRX(TransferReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllTransfer);
    LRX(BlitReadWrite, MemoryAccess::ReadWrite, PipelineStage::Blit);
    LRX(HostReadWrite, MemoryAccess::ReadWrite, PipelineStage::Host);
#undef LRX
}  // namespace PipelineAccess

}  // namespace lr::vk
