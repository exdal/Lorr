#include "Graphics/Renderer/Pass.hh"

namespace lr
{
void Graphics::AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name)
{
    pGraph->CreateImage("$backbuffer", nullptr, MemoryAccess::None);

    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPassCb<EmptyPassData>(
        name,
        [pGraph](Context *pContext, EmptyPassData &data, RenderPassBuilder &builder)
        {
            builder.SetInputResource("$backbuffer", { MemoryAccess::None, MemoryAccess::ColorAttachmentWrite });
            builder.SetColorAttachment("$backbuffer", { AttachmentOp::Clear, AttachmentOp::Store });
        });

    pPass->m_Flags |= RenderPassFlag::SkipRendering;
}

void Graphics::AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPassCb<EmptyPassData>(
        name,
        [pGraph](Context *pContext, EmptyPassData &data, RenderPassBuilder &builder)
        {
            builder.SetInputResource(
                "$backbuffer",
                { MemoryAccess::ColorAttachmentWrite, MemoryAccess::Present },
                InputResourceFlag::Present);
        });

    pPass->m_Flags |= RenderPassFlag::SkipRendering;
}

}  // namespace lr