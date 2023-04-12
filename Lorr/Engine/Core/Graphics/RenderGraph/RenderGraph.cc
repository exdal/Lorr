#include "RenderGraph.hh"

namespace lr::Graphics
{
static MemoryAccess ToMemoryAccess(ImageLayout layout, bool write)
{
    constexpr static MemoryAccess kMemoryAccess[] = {
        LR_MEMORY_ACCESS_NONE,                   // LR_IMAGE_LAYOUT_UNDEFINED
        LR_MEMORY_ACCESS_NONE,                   // LR_IMAGE_LAYOUT_PRESENT
        LR_MEMORY_ACCESS_COLOR_ATTACHMENT_READ,  // LR_IMAGE_LAYOUT_ATTACHMENT
        LR_MEMORY_ACCESS_SHADER_READ,            // LR_IMAGE_LAYOUT_READ_ONLY
        LR_MEMORY_ACCESS_TRANSFER_READ,          // LR_IMAGE_LAYOUT_TRANSFER_SRC
        LR_MEMORY_ACCESS_TRANSFER_READ,          // LR_IMAGE_LAYOUT_TRANSFER_DST -- `write` handle this
        LR_MEMORY_ACCESS_SHADER_READ,            // LR_IMAGE_LAYOUT_GENERAL
    };
    return kMemoryAccess[layout] << (MemoryAccess)write;
}

static ImageLayout ToImageLayout(MemoryAccess access)
{
    ZoneScoped;

    if (access & LR_MEMORY_ACCESS_COLOR_ATTACHMENT_UTL)
        return LR_IMAGE_LAYOUT_ATTACHMENT;
    if (access & LR_MEMORY_ACCESS_SHADER_READ || access & LR_MEMORY_ACCESS_DEPTH_STENCIL_READ)
        return LR_IMAGE_LAYOUT_READ_ONLY;
    if (access & LR_MEMORY_ACCESS_TRANSFER_READ)
        return LR_IMAGE_LAYOUT_TRANSFER_SRC;
    if (access & LR_MEMORY_ACCESS_TRANSFER_WRITE)
        return LR_IMAGE_LAYOUT_TRANSFER_DST;
    if (access & LR_MEMORY_ACCESS_SHADER_WRITE)
        return LR_IMAGE_LAYOUT_GENERAL;
    if (access & LR_MEMORY_ACCESS_PRESENT)
        return LR_IMAGE_LAYOUT_PRESENT;

    return LR_IMAGE_LAYOUT_UNDEFINED;
}

static PipelineStage ToPipelineStage(ImageLayout layout)
{
    ZoneScoped;

    constexpr static PipelineStage kColorStages[] = {
        LR_PIPELINE_STAGE_NONE,              // LR_IMAGE_LAYOUT_UNDEFINED
        LR_PIPELINE_STAGE_NONE,              // LR_IMAGE_LAYOUT_PRESENT
        LR_PIPELINE_STAGE_COLOR_ATTACHMENT,  // LR_IMAGE_LAYOUT_ATTACHMENT
        LR_PIPELINE_STAGE_PIXEL_SHADER,      // LR_IMAGE_LAYOUT_READ_ONLY
        LR_PIPELINE_STAGE_TRANSFER,          // LR_IMAGE_LAYOUT_TRANSFER_SRC
        LR_PIPELINE_STAGE_TRANSFER,          // LR_IMAGE_LAYOUT_TRANSFER_DST
        LR_PIPELINE_STAGE_COMPUTE_SHADER,    // LR_IMAGE_LAYOUT_GENERAL
    };

    return kColorStages[layout];
}

void RenderGraph::Init(RenderGraphDesc *pDesc)
{
    ZoneScoped;

    m_pContext = new VKContext;
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
        for (u32 t = 0; t < LR_COMMAND_LIST_TYPE_COUNT; t++)
        {
            CommandAllocator *pAllocator = m_pContext->CreateCommandAllocator((CommandListType)t, true);
            m_pContext->SetObjectName(pAllocator, Format("RG Command Allocator {}:{}", t, i));
            m_CommandAllocators.push_back(pAllocator);
        }

        Semaphore *pSema = m_pContext->CreateSemaphore(0, false);
        m_pContext->SetObjectName(pSema, Format("RG Semaphore {}", i));
        m_Semaphores.push_back(pSema);
    }

    AllocateCommandLists(LR_COMMAND_LIST_TYPE_GRAPHICS);
}

void RenderGraph::Shutdown()
{
    ZoneScoped;
}

void RenderGraph::Prepare()
{
    ZoneScoped;

    CommandAllocator *pAllocator = GetCommandAllocator(LR_COMMAND_LIST_TYPE_GRAPHICS);
    m_pSetupList = m_pContext->CreateCommandList(LR_COMMAND_LIST_TYPE_GRAPHICS, pAllocator);

    for (RenderPass *pPass : m_Passes)
    {
        if (pPass->m_Flags & LR_RENDER_PASS_FLAG_ALLOW_SETUP)
        {
            if (pPass->m_PassType == LR_COMMAND_LIST_TYPE_GRAPHICS)
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
}

void RenderGraph::Draw()
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    SwapChainFrame *pCurrentFrame = pSwapChain->GetCurrentFrame();
    Semaphore *pCurrentSemp = GetSemaphore();

    m_pContext->BeginFrame();
    m_pContext->WaitForSemaphore(pCurrentSemp, pCurrentSemp->m_Value);
    m_pContext->ResetCommandAllocator(GetCommandAllocator(LR_COMMAND_LIST_TYPE_GRAPHICS));

    CommandList *pList = GetCommandList(0);
    m_pContext->BeginCommandList(pList);

    for (RenderPass *pPass : m_Passes)
        RecordGraphicsPass((GraphicsRenderPass *)pPass, pList);

    m_pContext->EndCommandList(pList);

    CommandListSubmitDesc listSubmitDesc = { pList };
    SemaphoreSubmitDesc signalSemaDesc = {
        pCurrentSemp,
        ++pCurrentSemp->m_Value,
        LR_PIPELINE_STAGE_COLOR_ATTACHMENT,
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
    m_LastAccessInfos.push_back(LR_MEMORY_ACCESS_NONE);

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
            case LR_IMAGE_LAYOUT_ATTACHMENT:
            case LR_IMAGE_LAYOUT_READ_ONLY:
                return m_pContext->GetQueueIndex(LR_COMMAND_LIST_TYPE_GRAPHICS);
            case LR_IMAGE_LAYOUT_TRANSFER_SRC:
            case LR_IMAGE_LAYOUT_TRANSFER_DST:
                return m_pContext->GetQueueIndex(LR_COMMAND_LIST_TYPE_TRANSFER);
            case LR_IMAGE_LAYOUT_GENERAL:
                return m_pContext->GetQueueIndex(LR_COMMAND_LIST_TYPE_COMPUTE);
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
            .m_SrcStage = ToPipelineStage(lastLayout),
            .m_DstStage = ToPipelineStage(nextLayout),
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
        .m_UsageFlags = LR_BUFFER_USAGE_TRANSFER_SRC,
        .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME,
        .m_DataLen = dataSize,
    };
    Buffer *pBuffer = m_pContext->CreateBuffer(&bufferDesc);

    void *pMapData = nullptr;
    m_pContext->MapMemory(pBuffer, pMapData);
    memcpy(pMapData, pData, bufferDesc.m_DataLen);
    m_pContext->UnmapMemory(pBuffer);

    m_pContext->BeginCommandList(m_pSetupList);

    SetImageBarrier(m_pSetupList, pImage, LR_MEMORY_ACCESS_NONE, LR_MEMORY_ACCESS_TRANSFER_WRITE);
    m_pSetupList->CopyBuffer(pBuffer, pImage, LR_IMAGE_LAYOUT_TRANSFER_DST);
    SetImageBarrier(m_pSetupList, pImage, LR_MEMORY_ACCESS_TRANSFER_WRITE, resultAccess);

    m_pContext->EndCommandList(m_pSetupList);
    SubmitList(m_pSetupList, LR_PIPELINE_STAGE_ALL_COMMANDS, LR_PIPELINE_STAGE_ALL_COMMANDS);

    m_pContext->DeleteBuffer(pBuffer);
}

void RenderGraph::BuildPassBarriers(RenderPass *pPass)
{
    ZoneScoped;

    pPass->m_BarrierIndex = m_Barriers.size();

    for (RenderPassAttachment &colorAttachment : pPass->m_InputAttachments)
    {
        Image *pAttachment = GetAttachmentImage(colorAttachment);

        MemoryAccess lastAccess = GetResourceAccess(colorAttachment.m_Hash);
        ImageLayout lastLayout = ToImageLayout(lastAccess);
        ImageLayout nextLayout = ToImageLayout(colorAttachment.m_Access);
        MemoryAccess nextAccess =
            colorAttachment.m_Access | ToMemoryAccess(nextLayout, colorAttachment.HasClear());

        ImageBarrier barrier = {
            pAttachment,
            {
                .m_SrcLayout = lastLayout,
                .m_DstLayout = nextLayout,
                .m_SrcStage = ToPipelineStage(lastLayout),
                .m_DstStage = ToPipelineStage(nextLayout),
                .m_SrcAccess = lastAccess,
                .m_DstAccess = nextAccess,
            },
        };

        m_Barriers.push_back(barrier);
        SetMemoryAccess(colorAttachment.m_Hash, nextAccess);
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

    pPipelineInfo->m_BlendAttachments = pPass->m_BlendAttachments;
    pPass->m_pPipeline = m_pContext->CreateGraphicsPipeline(pPipelineInfo);
}

void RenderGraph::RecordGraphicsPass(GraphicsRenderPass *pPass, CommandList *pList)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();

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
            .m_Layout = LR_IMAGE_LAYOUT_ATTACHMENT,
            .m_LoadOp = attachment.HasClear() ? LR_ATTACHMENT_OP_CLEAR : LR_ATTACHMENT_OP_LOAD,
            .m_StoreOp = attachment.IsWrite() ? LR_ATTACHMENT_OP_STORE : LR_ATTACHMENT_OP_DONT_CARE,
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

void RenderGraph::SetMemoryAccess(Hash64 resource, MemoryAccess access)
{
    ZoneScoped;

    u32 idx = 0;
    for (auto &v : m_Images)
        if (v.Hash() == resource)
            break;
        else
            idx++;

    m_LastAccessInfos[idx] = access;
}

MemoryAccess RenderGraph::GetResourceAccess(Hash64 resource)
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

void RenderGraph::AllocateCommandLists(CommandListType type)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        CommandAllocator *pAllocator = m_CommandAllocators[i * pSwapChain->m_FrameCount + type];
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

CommandAllocator *RenderGraph::GetCommandAllocator(CommandListType type)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    u32 frameIdx = pSwapChain->m_CurrentFrame;
    u32 frameCount = pSwapChain->m_FrameCount;

    return m_CommandAllocators[frameIdx * frameCount + type];
}

CommandList *RenderGraph::GetCommandList(u32 submitID)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    u32 frameIdx = pSwapChain->m_CurrentFrame;
    u32 frameCount = pSwapChain->m_FrameCount;

    return m_CommandLists[(submitID * frameCount) + frameIdx];
}

}  // namespace lr::Graphics