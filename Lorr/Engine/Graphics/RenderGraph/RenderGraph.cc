// Created on Friday February 24th 2023 by exdal
// Last modified on Sunday June 25th 2023 by exdal

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
        PipelineStage::BottomOfPipe,           // LR_IMAGE_LAYOUT_PRESENT
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

    for (u32 frameIdx = 0; frameIdx < LR_MAX_FRAME_COUNT; frameIdx++)
    {
        u32 offset = frameIdx * LR_MAX_FRAME_COUNT;
        for (u32 i = 0; i < (u32)CommandType::Count; i++)
        {
            CommandAllocator *pAllocator = m_pContext->CreateCommandAllocator((CommandType)i, true);
            m_pContext->SetObjectName(pAllocator, _FMT("RG Command Allocator {}:{}", frameIdx, i));
            m_CommandAllocators[offset + i] = pAllocator;

            CommandList *pList = m_pContext->CreateCommandList((CommandType)i, pAllocator);
            m_pContext->SetObjectName(pList, _FMT("RG Command List {}:{}", frameIdx, i));
            m_CommandLists[offset + i] = pList;

            Semaphore *pSema = m_pContext->CreateSemaphore(0, false);
            m_pContext->SetObjectName(pSema, _FMT("RG Semaphore {}:{}", frameIdx, i));
            m_Semaphores[offset + i] = pSema;
        }
    }
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
    CommandList *pSetupList = GetCommandList(CommandType::Graphics);

    for (RenderPass *pPass : m_Passes)
    {
        builder.BuildPass(pPass);
    }

    m_pContext->BeginCommandList(pSetupList);

    /// RESOURCE DESCRIPTORS ///

    u64 descriptorBufferSize = builder.GetResourceBufferSize();
    if (descriptorBufferSize != 0)
    {
        BufferDesc resourceBufferDesc = {
            .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferDst,
            .m_TargetAllocator = ResourceAllocator::BufferTLSF,
            .m_DataSize = descriptorBufferSize,
        };
        m_pResourceDescriptorBuffer = m_pContext->CreateBuffer(&resourceBufferDesc);

        builder.GetResourceDescriptors(m_pResourceDescriptorBuffer, pSetupList);

        u32 resourceDescriptorCount = 0;
        builder.GetResourceDescriptorOffsets(nullptr, &resourceDescriptorCount);
        m_DescriptorSetOffsets.resize(resourceDescriptorCount);
        builder.GetResourceDescriptorOffsets(&m_DescriptorSetOffsets[0], &resourceDescriptorCount);

        m_DescriptorSetIndices.resize(resourceDescriptorCount);
        memset(&m_DescriptorSetIndices[0], 0, m_DescriptorSetIndices.size() * sizeof(u32));
    }

    /// SAMPLER DESCRIPTORS ///

    u64 samplerBufferSize = builder.GetSamplerBufferSize();
    if (samplerBufferSize != 0)
    {
        BufferDesc samplerBufferDesc = {
            .m_UsageFlags = BufferUsage::SamplerDescriptor | BufferUsage::TransferDst,
            .m_TargetAllocator = ResourceAllocator::BufferTLSF,
            .m_DataSize = samplerBufferSize,
        };
        m_pSamplerDescriptorBuffer = m_pContext->CreateBuffer(&samplerBufferDesc);

        builder.GetSamplerDescriptors(m_pSamplerDescriptorBuffer, pSetupList);

        m_DescriptorSetOffsets.push_back(0);  // It's only one buffer, does not matter
        m_DescriptorSetIndices.push_back(1);  // Again, we have 2 buffers
    }

    m_pContext->EndCommandList(pSetupList);
    SubmitList(pSetupList, PipelineStage::AllCommands, PipelineStage::AllCommands);
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

    CommandList *pList = GetCommandList(CommandType::Graphics);
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

void RenderGraph::FillImageData(Image *pImage, eastl::span<u8> imageData)
{
    ZoneScoped;

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::BufferFrametime,
        .m_DataSize = imageData.size_bytes(),
    };
    Buffer *pImageBuffer = m_pContext->CreateBuffer(&bufferDesc);

    void *pMapData = nullptr;
    m_pContext->MapBuffer(pImageBuffer, pMapData, 0, bufferDesc.m_DataSize);
    memcpy(pMapData, imageData.data(), bufferDesc.m_DataSize);
    m_pContext->UnmapBuffer(pImageBuffer);

    CommandList *pList = GetCommandList(CommandType::Graphics);
    m_pContext->BeginCommandList(pList);

    PipelineBarrier barrier = {
        .m_SrcLayout = ImageLayout::Undefined,
        .m_DstLayout = ImageLayout::TransferDst,
        .m_SrcStage = PipelineStage::None,
        .m_DstStage = PipelineStage::Copy,
        .m_SrcAccess = MemoryAccess::None,
        .m_DstAccess = MemoryAccess::TransferWrite,
    };
    ImageBarrier imageBarrier(pImage, ImageUsage::AspectColor, barrier);
    DependencyInfo depInfo(imageBarrier);
    pList->SetPipelineBarrier(&depInfo);

    pList->CopyBufferToImage(pImageBuffer, pImage, ImageUsage::AspectColor, ImageLayout::TransferDst);

    m_pContext->EndCommandList(pList);
    SubmitList(pList, PipelineStage::AllCommands, PipelineStage::AllCommands);

    m_pContext->DeleteBuffer(pImageBuffer, false);
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
        barriers.push_back({ pImage, ImageUsage::AspectColor, barrier });
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

        u32 setCount = m_DescriptorSetOffsets.size();
        DescriptorBindingInfo pBindingInfos[] = {
            { m_pResourceDescriptorBuffer, BufferUsage::ResourceDescriptor },
            { m_pSamplerDescriptorBuffer, BufferUsage::SamplerDescriptor },
        };
        pList->SetDescriptorBuffers(pBindingInfos);
        pList->SetDescriptorBufferOffsets(0, setCount, m_DescriptorSetIndices, m_DescriptorSetOffsets);

        pPass->Execute(m_pContext, pList);
    }

    pList->EndRendering();
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

    return m_CommandAllocators[(u32)type + (frameIdx * LR_MAX_FRAME_COUNT)];
}

CommandList *RenderGraph::GetCommandList(CommandType type)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pContext->GetSwapChain();
    u32 frameIdx = pSwapChain->m_CurrentFrame;

    return m_CommandLists[(u32)type + (frameIdx * LR_MAX_FRAME_COUNT)];
}

}  // namespace lr::Graphics