#include "Core/Graphics/Renderer/Pass.hh"

namespace lr
{
void Graphics::AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name)
{
    pGraph->CreateImage("$backbuffer", nullptr, LR_MEMORY_ACCESS_NONE);

    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name, LR_RENDER_PASS_FLAG_NONE);
    pPass->SetColorAttachment("$backbuffer", LR_MEMORY_ACCESS_COLOR_ATTACHMENT_WRITE, LR_ATTACHMENT_FLAG_CLEAR);
}

void Graphics::AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass =
        pGraph->CreateGraphicsPass<EmptyPassData>(name, LR_RENDER_PASS_FLAG_SKIP_RENDERING);
    pPass->SetColorAttachment("$backbuffer", LR_MEMORY_ACCESS_PRESENT, LR_ATTACHMENT_FLAG_PRESENT);
}

}  // namespace lr