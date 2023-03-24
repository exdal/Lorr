#include "Core/Graphics/Renderer/Pass.hh"

namespace lr
{
void Graphics::AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name);
    pPass->AddColorAttachment("$backbuffer", LR_MEMORY_ACCESS_RENDER_TARGET_WRITE, LR_ATTACHMENT_FLAG_NONE);
}

void Graphics::AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name);
    pPass->AddColorAttachment("$backbuffer", LR_MEMORY_ACCESS_NONE, LR_ATTACHMENT_FLAG_PRESENT);
}

}  // namespace lr