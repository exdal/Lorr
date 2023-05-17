// Created on Friday February 24th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#include "RenderGraph.hh"

#include "STL/Vector.hh"

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

    Memory::LinearAllocatorDesc allocatorDesc = {
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

    RenderPassBuilder builder(this);

    CommandAllocator *pAllocator = GetCommandAllocator(CommandType::Graphics);
    m_pSetupList = m_pContext->CreateCommandList(CommandType::Graphics, pAllocator);

    for (RenderPass *pPass : m_Passes)
    {
        builder.BuildPass(pPass);
    }

    // BufferDesc resourceBufferDesc = {
    //     .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferDst,
    //     .m_TargetAllocator = ResourceAllocator::BufferTLSF,
    //     .m_DataLen = builder.GetResourceDescriptorOffset(),
    // };
    // m_pResourceDescriptorBuffer = m_pContext->CreateBuffer(&resourceBufferDesc);

    // BufferDesc samplerBufferDesc = {
    //     .m_UsageFlags = BufferUsage::SamplerDescriptor | BufferUsage::TransferDst,
    //     .m_TargetAllocator = ResourceAllocator::BufferTLSF,
    //     .m_DataLen = builder.GetSamplerDescriptorOffset(),
    // };
    // m_pSamplerDescriptorBuffer = m_pContext->CreateBuffer(&samplerBufferDesc);

    // m_pContext->BeginCommandList(m_pSetupList);
    // m_pSetupList->CopyBuffer(
    //     builder.GetResourceDescriptorBuffer(), m_pResourceDescriptorBuffer, resourceBufferDesc.m_DataLen);
    // m_pSetupList->CopyBuffer(
    //     builder.GetSamplerDescriptorBuffer(), m_pSamplerDescriptorBuffer, samplerBufferDesc.m_DataLen);
    // m_pContext->EndCommandList(m_pSetupList);
    // SubmitList(m_pSetupList, PipelineStage::AllCommands, PipelineStage::AllCommands);
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

Image *RenderGraph::GetInputImage(const RenderPassInput &input)
{
    ZoneScoped;

    if (input.m_Hash == FNV64HashOf("$backbuffer"))
        return m_pContext->GetSwapChain()->GetCurrentFrame()->m_pImage;

    for (auto &[hash, resource] : m_Images)
        if (hash == input.m_Hash)
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

void RenderGraph::SetImageBarrier(CommandList *pList, Image *pImage, ImageBarrier &barrier)
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

    DependencyInfo depInfo = { barrier };
    pList->SetPipelineBarrier(&depInfo);
}

void RenderGraph::RecordGraphicsPass(GraphicsRenderPass *pPass, CommandList *pList)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();

    /// PREPARE ATTACHMENTS ///
    eastl::span<RenderPassInput> inputResources = GetPassResources(pPass);
    eastl::vector<RenderingColorAttachment> colorAttachments;
    eastl::vector<ImageBarrier> barriers;

    colorAttachments.reserve(pPass->m_ResourceCount);
    barriers.reserve(pPass->m_ResourceCount);

    for (RenderPassInput &inputResource : inputResources)
    {
        Image *pImage = GetInputImage(inputResource);

        if (inputResource.m_Flags & InputResourceFlag::ColorAttachment)
        {
            RenderingColorAttachment attachment = {
                .m_pImage = pImage,
                .m_Layout = ImageLayout::ColorAttachment,
                .m_LoadOp = inputResource.m_AttachmentOp.m_LoadOp,
                .m_StoreOp = inputResource.m_AttachmentOp.m_Store,
                .m_ClearValue = inputResource.m_ColorClearVal,
            };
            colorAttachments.push_back(attachment);

            continue;
        }

        PipelineBarrier barrier = {};
        barrier.m_SrcLayout = ToImageLayout(inputResource.m_Access.m_SrcAccess);
        barrier.m_DstLayout = ToImageLayout(inputResource.m_Access.m_DstAccess);
        barrier.m_SrcStage = ToImagePipelineStage(barrier.m_SrcLayout);
        barrier.m_DstStage = ToImagePipelineStage(barrier.m_DstLayout);
        barrier.m_SrcAccess = inputResource.m_Access.m_SrcAccess;
        barrier.m_DstAccess = inputResource.m_Access.m_DstAccess;
        barriers.push_back({ pImage, barrier });
    }

    DependencyInfo depInfo(barriers);
    pList->SetPipelineBarrier(&depInfo);

    if (pPass->m_Flags & RenderPassFlag::SkipRendering)
        return;

    RenderingBeginDesc renderingDesc = {
        .m_RenderArea = { 0, 0, pSwapChain->m_Width, pSwapChain->m_Height },
        .m_ColorAttachments = colorAttachments,
    };

    pList->BeginRendering(&renderingDesc);

    if (pPass->m_Flags & RenderPassFlag::AllowExecute)
    {
        pList->SetPipeline(pPass->m_pPipeline);

        DescriptorBindingInfo pBindingInfos[] = {
            { m_pResourceDescriptorBuffer, BufferUsage::ResourceDescriptor },
            { m_pSamplerDescriptorBuffer, BufferUsage::SamplerDescriptor },
        };
        pList->SetDescriptorBuffers(pBindingInfos);

        pPass->Execute(m_pContext, pList);
    }

    pList->EndRendering();
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