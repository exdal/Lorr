#include "Core/Graphics/Renderer/Pass.hh"

namespace lr
{
void Graphics::AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name);
    pGraph->SetCallbacks<EmptyPassData>(
        pPass,
        [&](RenderGraphResourceManager &, EmptyPassData &)
        {
        },
        [=](RenderGraphResourceManager &, EmptyPassData &, CommandList *)
        {
        },
        [](RenderGraphResourceManager &)
        {
        });

    RenderPassColorAttachment colorAttachment = {};
    colorAttachment.m_Flags = LR_ATTACHMENT_FLAG_NONE;
    colorAttachment.m_LoadOp = LR_ATTACHMENT_OP_CLEAR;
    colorAttachment.m_StoreOp = LR_ATTACHMENT_OP_STORE;
    colorAttachment.m_Usage = LR_IMAGE_USAGE_RENDER_TARGET;
    colorAttachment.m_ClearValue = ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f);
    pPass->AddColorAttachment("$backbuffer", colorAttachment);
}

void Graphics::AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<EmptyPassData>(name);

    RenderPassColorAttachment colorAttachment = {};
    colorAttachment.m_Flags = LR_ATTACHMENT_FLAG_NONE;
    colorAttachment.m_LoadOp = LR_ATTACHMENT_OP_DONT_CARE;
    colorAttachment.m_StoreOp = LR_ATTACHMENT_OP_DONT_CARE;
    colorAttachment.m_Usage = LR_IMAGE_USAGE_PRESENT;
    pPass->AddColorAttachment("$backbuffer", colorAttachment);
}

}  // namespace lr