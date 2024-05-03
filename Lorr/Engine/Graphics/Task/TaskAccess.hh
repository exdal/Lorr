#pragma once

#include "Graphics/Common.hh"

namespace lr::graphics::TaskAccess {
struct Access {
    constexpr Access() = default;
    constexpr Access(MemoryAccess access_, PipelineStage stage_)
        : access(access_),
          stage(stage_)
    {
    }

    MemoryAccess access = MemoryAccess::None;
    PipelineStage stage = PipelineStage::None;

    auto operator<=>(const Access &) const = default;
};

constexpr Access operator|(const Access &lhs, const Access &rhs)
{
    return { lhs.access | rhs.access, lhs.stage | rhs.stage };
}

constexpr Access operator|=(Access &lhs, const Access &rhs)
{
    Access lhsn = { lhs.access | rhs.access, lhs.stage | rhs.stage };
    return lhs = lhsn | rhs;
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
}  // namespace lr::graphics::TaskAccess
