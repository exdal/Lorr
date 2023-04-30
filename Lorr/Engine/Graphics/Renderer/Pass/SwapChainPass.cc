#include "Graphics/Renderer/Pass.hh"

namespace lr
{
void Graphics::AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name)
{
    pGraph->CreateImage("$backbuffer", nullptr, MemoryAccess::None);

    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name, LR_RENDER_PASS_FLAG_NONE);
    pPass->SetColorAttachment("$backbuffer", MemoryAccess::ColorAttachmentWrite, LR_ATTACHMENT_FLAG_CLEAR);
}

void Graphics::AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass =
        pGraph->CreateGraphicsPass<EmptyPassData>(name, LR_RENDER_PASS_FLAG_SKIP_RENDERING);
    pPass->SetColorAttachment("$backbuffer", MemoryAccess::Present, LR_ATTACHMENT_FLAG_PRESENT);
}

}  // namespace lr