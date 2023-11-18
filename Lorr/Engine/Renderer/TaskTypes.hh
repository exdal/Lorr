#pragma once

#include "Graphics/Common.hh"

namespace lr::Renderer
{
namespace TaskAccess
{
    using namespace Graphics;
    struct Access
    {
        constexpr Access() = default;
        constexpr Access(MemoryAccess access, PipelineStage stage)
            : m_Access(access),
              m_Stage(stage){};
        MemoryAccess m_Access = MemoryAccess::None;
        PipelineStage m_Stage = PipelineStage::None;

        auto operator<=>(const Access &) const = default;
    };

    constexpr Access operator|(const Access &lhs, const Access &rhs)
    {
        return (Access){ lhs.m_Access | rhs.m_Access, lhs.m_Stage | rhs.m_Stage };
    }

    // clang-format off
#define DECLARE_ACCESS(name, access, stages) constexpr static Access name(access, stages)
    DECLARE_ACCESS(None, MemoryAccess::None, PipelineStage::None);
    DECLARE_ACCESS(TopOfPipe, MemoryAccess::None, PipelineStage::TopOfPipe);
    DECLARE_ACCESS(BottomOfPipe, MemoryAccess::None, PipelineStage::BottomOfPipe);
    DECLARE_ACCESS(VertexAttrib, MemoryAccess::Read, PipelineStage::VertexAttribInput);
    DECLARE_ACCESS(IndexAttrib, MemoryAccess::Read, PipelineStage::IndexAttribInput);

    DECLARE_ACCESS(IndirectRead, MemoryAccess::Read, PipelineStage::DrawIndirect);
    DECLARE_ACCESS(VertexShaderRead, MemoryAccess::Read, PipelineStage::VertexShader);
    DECLARE_ACCESS(TessControlRead, MemoryAccess::Read, PipelineStage::TessellationControl);
    DECLARE_ACCESS(TessEvalRead, MemoryAccess::Read, PipelineStage::TessellationEvaluation);
    DECLARE_ACCESS(PixelShaderRead, MemoryAccess::Read, PipelineStage::PixelShader);
    DECLARE_ACCESS(EarlyPixelTestsRead, MemoryAccess::Read, PipelineStage::EarlyPixelTests);
    DECLARE_ACCESS(LatePixelTestsRead, MemoryAccess::Read, PipelineStage::LatePixelTests);
    DECLARE_ACCESS(ColorAttachmentRead, MemoryAccess::Read, PipelineStage::ColorAttachmentOutput);
    DECLARE_ACCESS(GraphicsRead, MemoryAccess::Read, PipelineStage::AllGraphics);
    DECLARE_ACCESS(ComputeRead, MemoryAccess::Read, PipelineStage::ComputeShader);
    DECLARE_ACCESS(TransferRead, MemoryAccess::Read, PipelineStage::AllTransfer);
    DECLARE_ACCESS(HostRead, MemoryAccess::Read, PipelineStage::Host);

    DECLARE_ACCESS(IndirectWrite, MemoryAccess::Write, PipelineStage::DrawIndirect);
    DECLARE_ACCESS(VertexShaderWrite, MemoryAccess::Write, PipelineStage::VertexShader);
    DECLARE_ACCESS(TessControlWrite, MemoryAccess::Write, PipelineStage::TessellationControl);
    DECLARE_ACCESS(TessEvalWrite, MemoryAccess::Write, PipelineStage::TessellationEvaluation);
    DECLARE_ACCESS(PixelShaderWrite, MemoryAccess::Write, PipelineStage::PixelShader);
    DECLARE_ACCESS(EarlyPixelTestsWrite, MemoryAccess::Write, PipelineStage::EarlyPixelTests);
    DECLARE_ACCESS(LatePixelTestsWrite, MemoryAccess::Write, PipelineStage::LatePixelTests);
    DECLARE_ACCESS(ColorAttachmentWrite, MemoryAccess::Write, PipelineStage::ColorAttachmentOutput);
    DECLARE_ACCESS(GraphicsWrite, MemoryAccess::Write, PipelineStage::AllGraphics);
    DECLARE_ACCESS(ComputeWrite, MemoryAccess::Write, PipelineStage::ComputeShader);
    DECLARE_ACCESS(TransferWrite, MemoryAccess::Write, PipelineStage::AllTransfer);
    DECLARE_ACCESS(HostWrite, MemoryAccess::Write, PipelineStage::Host);

    DECLARE_ACCESS(IndirectReadWrite, MemoryAccess::ReadWrite, PipelineStage::DrawIndirect);
    DECLARE_ACCESS(VertexShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::VertexShader);
    DECLARE_ACCESS(TessControlReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationControl);
    DECLARE_ACCESS(TessEvalReadWrite, MemoryAccess::ReadWrite, PipelineStage::TessellationEvaluation);
    DECLARE_ACCESS(PixelShaderReadWrite, MemoryAccess::ReadWrite, PipelineStage::PixelShader);
    DECLARE_ACCESS(EarlyPixelTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::EarlyPixelTests);
    DECLARE_ACCESS(LatePixelTestsReadWrite, MemoryAccess::ReadWrite, PipelineStage::LatePixelTests);
    DECLARE_ACCESS(ColorAttachmentReadWrite, MemoryAccess::ReadWrite, PipelineStage::ColorAttachmentOutput);
    DECLARE_ACCESS(GraphicsReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllGraphics);
    DECLARE_ACCESS(ComputeReadWrite, MemoryAccess::ReadWrite, PipelineStage::ComputeShader);
    DECLARE_ACCESS(TransferReadWrite, MemoryAccess::ReadWrite, PipelineStage::AllTransfer);
    DECLARE_ACCESS(HostReadWrite, MemoryAccess::ReadWrite, PipelineStage::Host);

#undef DECLARE_ACCESS
    // clang-format on
}  // namespace TaskAccess

enum class ResourceType : u32
{
    Buffer,
    Image,
};

enum class ResourceFlag : u32
{
    None = 0,
    SwapChainImage = 1 << 0,
    SwapChainRelative = 1 << 1,  // for scaling
};
LR_TYPEOP_ARITHMETIC_INT(ResourceFlag, ResourceFlag, &);

template<typename _T>
struct ToResourceType;

template<>
struct ToResourceType<Graphics::Image>
{
    constexpr static ResourceType kType = ResourceType::Image;
};

template<>
struct ToResourceType<Graphics::Buffer>
{
    constexpr static ResourceType kType = ResourceType::Buffer;
};
}  // namespace lr::Renderer