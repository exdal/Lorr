#include "RenderGraph.hh"

namespace lr::Graphics
{
static MemoryAccess ToImageMemoryAccess(ImageLayout layout, bool write)
{
    constexpr static MemoryAccess kMemoryAccess[] = {
        MemoryAccess::None,                 // LR_IMAGE_LAYOUT_UNDEFINED
        MemoryAccess::None,                 // LR_IMAGE_LAYOUT_PRESENT
        MemoryAccess::ColorAttachmentRead,  // LR_IMAGE_LAYOUT_COLOR_ATTACHMENT
        MemoryAccess::DepthStencilRead,     // LR_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT
        MemoryAccess::SampledRead,          // LR_IMAGE_LAYOUT_COLOR_READ_ONLY
        MemoryAccess::SampledRead,          // LR_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY
        MemoryAccess::TransferRead,         // LR_IMAGE_LAYOUT_TRANSFER_SRC
        MemoryAccess::TransferRead,         // LR_IMAGE_LAYOUT_TRANSFER_DST
        MemoryAccess::StorageRead,          // LR_IMAGE_LAYOUT_GENERAL
    };
    return kMemoryAccess[(u32)layout] << (MemoryAccess)write;
}

// TODO: Depth attachemtns
static ImageLayout ToImageLayout(MemoryAccess access)
{
    ZoneScoped;

    if (access & MemoryAccess::ColorAttachmentAll)
        return ImageLayout::ColorAttachment;
    if (access & MemoryAccess::SampledRead)
        return ImageLayout::ColorReadOnly;
    if (access & MemoryAccess::TransferRead)
        return ImageLayout::TransferSrc;
    if (access & MemoryAccess::TransferWrite)
        return ImageLayout::TransferDst;
    if (access & (MemoryAccess::StorageWrite | MemoryAccess::StorageRead))
        return ImageLayout::General;
    if (access & MemoryAccess::Present)
        return ImageLayout::Present;

    return ImageLayout::Undefined;
}

static PipelineStage ToImagePipelineStage(ImageLayout layout)
{
    ZoneScoped;

    // TODO: EARLY(loadop) and LATE(storeop) tests

    constexpr static PipelineStage kColorStages[] = {
        PipelineStage::None,                   // LR_IMAGE_LAYOUT_UNDEFINED
        PipelineStage::None,                   // LR_IMAGE_LAYOUT_PRESENT
        PipelineStage::ColorAttachmentOutput,  // LR_IMAGE_LAYOUT_COLOR_ATTACHMENT
        PipelineStage::EarlyPixelTests,        // LR_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT
        PipelineStage::PixelShader,            // LR_IMAGE_LAYOUT_COLOR_READ_ONLY
        PipelineStage::PixelShader,            // LR_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY
        PipelineStage::Copy,                   // LR_IMAGE_LAYOUT_TRANSFER_SRC
        PipelineStage::Copy,                   // LR_IMAGE_LAYOUT_TRANSFER_DST
        PipelineStage::ComputeShader,          // LR_IMAGE_LAYOUT_GENERAL
    };

    return kColorStages[(u32)layout];
}

void RenderGraph::Init(RenderGraphDesc *pDesc)
{
    ZoneScoped;

    m_pContext = new Context;
    m_pContext->Init(&pDesc->m_APIDesc);

    /// INIT RESOURCES ///

    Memory::AllocatorDesc allocatorDesc = {
        .m_DataSize = Memory::KiBToBytes(64),
        .m_AutoGrowSize = Memory::KiBToBytes(6),
    };
    m_PassAllocator.Init(allocatorDesc);

    u32 frameCount = m_pContext->m_pSwapChain->m_FrameCount;
    for (u32 i = 0; i < frameCount; i++)
    {
        for (u32 t = 0; t < (u32)CommandType::Count; t++)
        {
            CommandAllocator *pAllocator = m_pContext->CreateCommandAllocator((CommandType)t, true);
            m_pContext->SetObjectName(pAllocator, Format("RG Command Allocator {}:{}", t, i));
            m_CommandAllocators.push_back(pAllocator);
        }

        Semaphore *pSema = m_pContext->CreateSemaphore(0, false);
        m_pContext->SetObjectName(pSema, Format("RG Semaphore {}", i));
        m_Semaphores.push_back(pSema);
    }

    AllocateCommandLists(CommandType::Graphics);
}

void RenderGraph::Shutdown()
{
    ZoneScoped;
}

void RenderGraph::Prepare()
{
    ZoneScoped;

    CommandAllocator *pAllocator = GetCommandAllocator(CommandType::Graphics);
    m_pSetupList = m_pContext->CreateCommandList(CommandType::Graphics, pAllocator);

    for (RenderPass *pPass : m_Passes)
    {
        if (pPass->m_Flags & LR_RENDER_PASS_FLAG_ALLOW_SETUP)
        {
            if (pPass->m_PassType == CommandType::Graphics)
            {
                GraphicsRenderPass *pGraphicsPass = (GraphicsRenderPass *)pPass;
                GraphicsPipelineBuildInfo pipelineInfo = {};

                pGraphicsPass->SetPipelineInfo(pipelineInfo);
                pGraphicsPass->Setup(m_pContext);

                FinalizeGraphicsPass(pGraphicsPass, &pipelineInfo);
            }
        }

        BuildPassBarriers(pPass);
    }

    u32 bufferSize = 0;
    for (DescriptorSetLayout *pLayout : m_DescriptorLayouts)
        bufferSize += m_pContext->GetDescriptorSetLayoutSize(pLayout);

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::ResourceDescriptor,
        .m_TargetAllocator = ResourceAllocator::BufferTLSF,
        .m_DataLen = bufferSize,
    };
    m_pDescriptorBuffer = m_pContext->CreateBuffer(&bufferDesc);

    m_pContext->BeginCommandList(m_pSetupList);

    DescriptorBindingInfo bindingInfo(nullptr, m_pDescriptorBuffer, BufferUsage::ResourceDescriptor);
    m_pSetupList->SetDescriptorBuffers(bindingInfo);

    m_pContext->EndCommandList(m_pSetupList);
    SubmitList(m_pSetupList, PipelineStage::AllCommands, PipelineStage::AllCommands);
}

void RenderGraph::Draw()
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();
    SwapChainFrame *pCurrentFrame = pSwapChain->GetCurrentFrame();
    Semaphore *pCurrentSemp = GetSemaphore();

    m_pContext->BeginFrame();
    m_pContext->WaitForSemaphore(pCurrentSemp, pCurrentSemp->m_Value);
    m_pContext->ResetCommandAllocator(GetCommandAllocator(CommandType::Graphics));

    CommandList *pList = GetCommandList(0);
    m_pContext->BeginCommandList(pList);

    for (RenderPass *pPass : m_Passes)
        RecordGraphicsPass((GraphicsRenderPass *)pPass, pList);

    m_pContext->EndCommandList(pList);

    CommandListSubmitDesc listSubmitDesc = { pList };
    SemaphoreSubmitDesc signalSemaDesc = {
        pCurrentSemp,
        ++pCurrentSemp->m_Value,
        PipelineStage::ColorAttachmentOutput,
    };

    SubmitDesc submitDesc = {
        .m_Type = pList->m_Type,
        .m_Lists = listSubmitDesc,
        .m_SignalSemas = signalSemaDesc,
    };
    m_pContext->Submit(&submitDesc);
    m_pContext->EndFrame();
}

Image *RenderGraph::GetAttachmentImage(const RenderPassAttachment &attachment)
{
    ZoneScoped;

    if (attachment.m_Hash == FNV64HashOf("$backbuffer"))
        return m_pContext->GetSwapChain()->GetCurrentFrame()->m_pImage;

    for (auto &[hash, resource] : m_Images)
        if (hash == attachment.m_Hash)
            return resource;

    return nullptr;
}

Image *RenderGraph::CreateImage(NameID name, ImageDesc &desc, MemoryAccess initialAccess)
{
    ZoneScoped;

    Image *pImage = m_pContext->CreateImage(&desc);
    m_pContext->SetObjectName(pImage, name);

    return CreateImage(name, pImage, initialAccess);
}

Image *RenderGraph::CreateImage(NameID name, Image *pImage, MemoryAccess initialAccess)
{
    ZoneScoped;

    ResourceView<Image> view = { name, pImage };
    m_Images.push_back(view);
    m_LastAccessInfos.push_back({ .m_Access = MemoryAccess::None });

    return pImage;
}

void RenderGraph::SubmitList(CommandList *pList, PipelineStage waitStage, PipelineStage signalStage)
{
    ZoneScoped;

    Semaphore *pSemaphore = GetSemaphore();
    CommandListSubmitDesc listSubmitDesc = { pList };
    SemaphoreSubmitDesc waitSemaDesc = { pSemaphore, pSemaphore->m_Value, waitStage };
    SemaphoreSubmitDesc signalSemaDesc = { pSemaphore, ++pSemaphore->m_Value, signalStage };

    SubmitDesc submitDesc = {
        .m_Type = pList->m_Type,
        .m_WaitSemas = waitSemaDesc,
        .m_Lists = listSubmitDesc,
        .m_SignalSemas = signalSemaDesc,
    };
    m_pContext->Submit(&submitDesc);

    m_pContext->WaitForSemaphore(pSemaphore, pSemaphore->m_Value);
    m_pContext->ResetCommandAllocator(GetCommandAllocator(pList->m_Type));
}

void RenderGraph::SetImageBarrier(
    CommandList *pList, Image *pImage, MemoryAccess srcAccess, MemoryAccess dstAccess)
{
    auto SelectQueue = [this](ImageLayout layout) -> u32
    {
        switch (layout)
        {
            case ImageLayout::ColorAttachment:
            case ImageLayout::ColorReadOnly:
            case ImageLayout::DepthStencilAttachment:
            case ImageLayout::DepthStencilReadOnly:
                return m_pContext->GetQueueIndex(CommandType::Graphics);
            case ImageLayout::TransferSrc:
            case ImageLayout::TransferDst:
                return m_pContext->GetQueueIndex(CommandType::Transfer);
            case ImageLayout::General:
                return m_pContext->GetQueueIndex(CommandType::Compute);
            default:
                return ~0;
        }
    };

    ImageLayout lastLayout = ToImageLayout(srcAccess);
    ImageLayout nextLayout = ToImageLayout(dstAccess);

    ImageBarrier barrier = {
        pImage,
        {
            .m_SrcLayout = lastLayout,
            .m_DstLayout = nextLayout,
            .m_SrcStage = ToImagePipelineStage(lastLayout),
            .m_DstStage = ToImagePipelineStage(nextLayout),
            .m_SrcAccess = srcAccess,
            .m_DstAccess = dstAccess,
        },
    };
    DependencyInfo depInfo = { barrier };
    pList->SetPipelineBarrier(&depInfo);
}

void RenderGraph::UploadImageData(Image *pImage, MemoryAccess resultAccess, void *pData, u32 dataSize)
{
    ZoneScoped;

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::BufferFrametime,
        .m_DataLen = dataSize,
    };
    Buffer *pBuffer = m_pContext->CreateBuffer(&bufferDesc);

    void *pMapData = nullptr;
    m_pContext->MapMemory(pBuffer, pMapData);
    memcpy(pMapData, pData, bufferDesc.m_DataLen);
    m_pContext->UnmapMemory(pBuffer);

    m_pContext->BeginCommandList(m_pSetupList);

    SetImageBarrier(m_pSetupList, pImage, MemoryAccess::None, MemoryAccess::TransferWrite);
    m_pSetupList->CopyBuffer(pBuffer, pImage, ImageLayout::TransferDst);
    SetImageBarrier(m_pSetupList, pImage, MemoryAccess::TransferWrite, resultAccess);

    m_pContext->EndCommandList(m_pSetupList);
    SubmitList(m_pSetupList, PipelineStage::AllCommands, PipelineStage::AllCommands);

    m_pContext->DeleteBuffer(pBuffer);
}

void RenderGraph::BuildPassBarriers(RenderPass *pPass)
{
    ZoneScoped;

    pPass->m_BarrierIndex = m_Barriers.size();

    for (RenderPassAttachment &colorAttachment : pPass->m_InputAttachments)
    {
        Image *pAttachment = GetAttachmentImage(colorAttachment);

        PipelineAccessInfo accessInfo = GetResourceAccessInfo(colorAttachment.m_Hash);
        ImageLayout lastLayout = ToImageLayout(accessInfo.m_Access);
        ImageLayout nextLayout = ToImageLayout(colorAttachment.m_Access);
        MemoryAccess nextAccess = ToImageMemoryAccess(nextLayout, colorAttachment.HasClear());

        ImageBarrier barrier = {
            pAttachment,
            {
                .m_SrcLayout = lastLayout,
                .m_DstLayout = nextLayout,
                .m_SrcStage = ToImagePipelineStage(lastLayout),
                .m_DstStage = ToImagePipelineStage(nextLayout),
                .m_SrcAccess = accessInfo.m_Access,
                .m_DstAccess = nextAccess,
            },
        };

        m_Barriers.push_back(barrier);
        SetMemoryAccess(colorAttachment.m_Hash, { .m_Access = nextAccess });
    }
}

void RenderGraph::FinalizeGraphicsPass(GraphicsRenderPass *pPass, GraphicsPipelineBuildInfo *pPipelineInfo)
{
    ZoneScoped;

    for (RenderPassAttachment &colorAttachment : pPass->m_InputAttachments)
    {
        Image *pAttachment = GetAttachmentImage(colorAttachment);
        pPipelineInfo->m_ColorAttachmentFormats.push_back(pAttachment->m_Format);
    }

    for (auto &pLayout : pPipelineInfo->m_Layouts)
        m_DescriptorLayouts.push_back(pLayout);

    pPipelineInfo->m_BlendAttachments = pPass->m_BlendAttachments;
    pPass->m_pPipeline = m_pContext->CreateGraphicsPipeline(pPipelineInfo);
}

void RenderGraph::RecordGraphicsPass(GraphicsRenderPass *pPass, CommandList *pList)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();

    /// PREPARE ATTACHMENTS ///
    eastl::span<RenderPassAttachment> passColorAttachments = pPass->m_InputAttachments;
    eastl::span<ImageBarrier> barriers(m_Barriers.begin() + pPass->m_BarrierIndex, passColorAttachments.size());

    eastl::vector<RenderingColorAttachment> renderingColorAttachments(passColorAttachments.size());

    for (u32 i = 0; i < passColorAttachments.size(); i++)
    {
        RenderPassAttachment &attachment = passColorAttachments[i];
        Image *pImage = GetAttachmentImage(attachment);

        renderingColorAttachments[i] = {
            .m_pImage = pImage,
            .m_Layout = ImageLayout::ColorAttachment,
            .m_LoadOp = attachment.HasClear() ? AttachmentOp::Clear : AttachmentOp::Load,
            .m_StoreOp = attachment.IsWrite() ? AttachmentOp::Store : AttachmentOp::DontCare,
            .m_ClearValue = attachment.m_ColorClearVal,
        };
    }

    DependencyInfo depInfo = barriers;
    pList->SetPipelineBarrier(&depInfo);

    if (pPass->m_Flags & LR_RENDER_PASS_FLAG_SKIP_RENDERING)
        return;

    RenderingBeginDesc renderingDesc = {
        .m_RenderArea = { 0, 0, pSwapChain->m_Width, pSwapChain->m_Height },
        .m_ColorAttachments = renderingColorAttachments,
    };

    pList->BeginRendering(&renderingDesc);

    if (pPass->m_Flags & LR_RENDER_PASS_FLAG_ALLOW_EXECUTE)
    {
        pList->SetPipeline(pPass->m_pPipeline);
        pPass->Execute(m_pContext, pList);
    }

    pList->EndRendering();
}

void RenderGraph::SetMemoryAccess(Hash64 resource, PipelineAccessInfo info)
{
    ZoneScoped;

    u32 idx = 0;
    for (auto &v : m_Images)
        if (v.Hash() == resource)
            break;
        else
            idx++;

    m_LastAccessInfos[idx] = info;
}

PipelineAccessInfo RenderGraph::GetResourceAccessInfo(Hash64 resource)
{
    ZoneScoped;

    u32 idx = 0;
    for (auto &v : m_Images)
        if (v.Hash() == resource)
            break;
        else
            idx++;

    return m_LastAccessInfos[idx];
}

void RenderGraph::AllocateCommandLists(CommandType type)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();
    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        CommandAllocator *pAllocator = m_CommandAllocators[i * pSwapChain->m_FrameCount + (u32)type];
        CommandList *pList = m_pContext->CreateCommandList(type, pAllocator);
        m_pContext->SetObjectName(pList, Format("List {}", m_CommandLists.size()));
        m_CommandLists.push_back(pList);
    }
}

Semaphore *RenderGraph::GetSemaphore()
{
    ZoneScoped;

    u32 frameIdx = m_pContext->GetSwapChain()->m_CurrentFrame;
    return m_Semaphores[frameIdx];
}

CommandAllocator *RenderGraph::GetCommandAllocator(CommandType type)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();
    u32 frameIdx = pSwapChain->m_CurrentFrame;
    u32 frameCount = pSwapChain->m_FrameCount;

    return m_CommandAllocators[frameIdx * frameCount + (u32)type];
}

CommandList *RenderGraph::GetCommandList(u32 submitID)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();
    u32 frameIdx = pSwapChain->m_CurrentFrame;
    u32 frameCount = pSwapChain->m_FrameCount;

    return m_CommandLists[(submitID * frameCount) + frameIdx];
}

}  // namespace lr::Graphics