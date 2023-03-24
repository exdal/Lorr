#include "RenderGraph.hh"

namespace lr::Graphics
{
void RenderGraph::Init(RenderGraphDesc *pDesc)
{
    ZoneScoped;

    Memory::AllocatorDesc allocatorDesc = {
        .m_DataSize = Memory::KiBToBytes(64),
        .m_AutoGrowSize = Memory::KiBToBytes(6),
    };
    m_PassAllocator.Init(allocatorDesc);

    allocatorDesc = {
        .m_DataSize = sizeof(RenderPassGroup) * LR_RG_MAX_GROUP_COUNT,
        .m_AutoGrowSize = 0,
    };
    m_GroupAllocator.Init(allocatorDesc);

    m_pContext = new VKContext;
    m_pContext->Init(&pDesc->m_APIDesc);

    for (u32 i = 0; i < LR_MAX_FRAME_COUNT; i++)
    {
        m_pCommandAllocators[i] = m_pContext->CreateCommandAllocator(LR_COMMAND_LIST_TYPE_GRAPHICS, true);
        m_pContext->SetObjectName(m_pCommandAllocators[i], Format("Render Graph Command Allocator {}", i));
        m_pSemaphores[i] = m_pContext->CreateSemaphore(0, false);
        m_pContext->SetObjectName(m_pSemaphores[i], Format("Render Graph Semaphore {}", i));
    }

    GetOrCreateGroup("$head");
}

void RenderGraph::Shutdown()
{
    ZoneScoped;
}

void RenderGraph::Draw()
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    SwapChainFrame *pCurrentFrame = pSwapChain->GetCurrentFrame();

    Semaphore *pCurrentSemp = m_pSemaphores[pSwapChain->m_CurrentFrame];
    CommandAllocator *pCurrentAllocator = m_pCommandAllocators[pSwapChain->m_CurrentFrame];

    m_pContext->BeginFrame();
    m_pContext->WaitForSemaphore(pCurrentSemp, pCurrentSemp->m_Value);
    m_pContext->ResetCommandAllocator(pCurrentAllocator);

    ExecuteGraphics(GetPass<GraphicsRenderPass>("$acquire"));

    RenderPassGroup *pHeadGroup = GetGroup("$head");
    u64 currentLevelMask = pHeadGroup->m_DependencyMask;
    while (currentLevelMask != 0)
    {
        // Execute current level
        u64 nextLevelMask = 0;
        u64 dependencyMask = currentLevelMask;
        while (dependencyMask != 0)
        {
            // Execute current group
            u32 groupID = Memory::GetLSB(dependencyMask);
            RenderPassGroup *pGroup = GetGroup(groupID);

            for (RenderPass *pPass : pGroup->m_Passes)
                ExecuteGraphics((GraphicsRenderPass *)pPass);

            nextLevelMask |= pGroup->m_DependencyMask;
            dependencyMask ^= 1 << groupID;
        }

        currentLevelMask = nextLevelMask;
    }

    ExecuteGraphics(GetPass<GraphicsRenderPass>("$present"));
    m_pContext->EndFrame();
}

void RenderGraph::AddGraphicsGroup(
    eastl::string_view name, eastl::string_view dependantGroup, cinitl<eastl::string_view> &passes)
{
    ZoneScoped;

    RenderPassGroup *pGroup = GetOrCreateGroup(name);
    RenderPassGroup *pDependantGroup = GetGroup(dependantGroup);
    assert(!(pGroup->m_Passes.full() && "Reached maximum pass count for a group."));

    if (pDependantGroup->m_ID != pGroup->m_ID)
        pDependantGroup->m_DependencyMask |= 1 << pGroup->m_ID;

    for (eastl::string_view passName : passes)
    {
        RenderPass *pPass = GetPass(passName);
        if (!pPass && pPass->m_BoundGroupID == LR_INVALID_GROUP_ID)
        {
            LOG_CRITICAL("Renderpass {} is either invalid or bound to another group.", passName);
            continue;
        }

        pPass->m_BoundGroupID = pGroup->m_ID;
        pGroup->m_Passes.push_back(pPass);
    }
}

static MemoryAccess ToMemoryAccess(ImageLayout layout, bool write)
{
    constexpr static MemoryAccess kMemoryAccess[] = {
        LR_MEMORY_ACCESS_NONE,                // LR_IMAGE_LAYOUT_UNDEFINED
        LR_MEMORY_ACCESS_NONE,                // LR_IMAGE_LAYOUT_PRESENT
        LR_MEMORY_ACCESS_RENDER_TARGET_READ,  // LR_IMAGE_LAYOUT_ATTACHMENT
        LR_MEMORY_ACCESS_SHADER_READ,         // LR_IMAGE_LAYOUT_READ_ONLY
        LR_MEMORY_ACCESS_TRANSFER_READ,       // LR_IMAGE_LAYOUT_TRANSFER_SRC
        LR_MEMORY_ACCESS_TRANSFER_READ,       // LR_IMAGE_LAYOUT_TRANSFER_DST -- `write` handle this
        LR_MEMORY_ACCESS_SHADER_READ,         // LR_IMAGE_LAYOUT_UNORDERED_ACCESS
    };
    return kMemoryAccess[layout] << (MemoryAccess)write;
}

static ImageLayout ToImageLayout(MemoryAccess access)
{
    ZoneScoped;

    if (access & LR_MEMORY_ACCESS_RENDER_TARGET_UTL)
        return LR_IMAGE_LAYOUT_ATTACHMENT;
    if (access & LR_MEMORY_ACCESS_SHADER_READ || access & LR_MEMORY_ACCESS_DEPTH_STENCIL_READ)
        return LR_IMAGE_LAYOUT_ATTACHMENT;
    if (access & LR_MEMORY_ACCESS_TRANSFER_READ)
        return LR_IMAGE_LAYOUT_TRANSFER_SRC;
    if (access & LR_MEMORY_ACCESS_TRANSFER_WRITE)
        return LR_IMAGE_LAYOUT_TRANSFER_DST;
    if (access & LR_MEMORY_ACCESS_SHADER_WRITE)
        return LR_IMAGE_LAYOUT_UNORDERED_ACCESS;

    return LR_IMAGE_LAYOUT_UNDEFINED;
}

static PipelineStage ToPipelineStage(ImageLayout layout)
{
    ZoneScoped;

    constexpr static PipelineStage kColorStages[] = {
        LR_PIPELINE_STAGE_NONE,            // LR_IMAGE_LAYOUT_UNDEFINED
        LR_PIPELINE_STAGE_NONE,            // LR_IMAGE_LAYOUT_PRESENT
        LR_PIPELINE_STAGE_RENDER_TARGET,   // LR_IMAGE_LAYOUT_ATTACHMENT
        LR_PIPELINE_STAGE_PIXEL_SHADER,    // LR_IMAGE_LAYOUT_READ_ONLY
        LR_PIPELINE_STAGE_TRANSFER,        // LR_IMAGE_LAYOUT_TRANSFER_SRC
        LR_PIPELINE_STAGE_TRANSFER,        // LR_IMAGE_LAYOUT_TRANSFER_DST
        LR_PIPELINE_STAGE_COMPUTE_SHADER,  // LR_IMAGE_LAYOUT_UNORDERED_ACCESS
    };

    return kColorStages[layout];
}

void RenderGraph::ExecuteGraphics(GraphicsRenderPass *pPass)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    CommandList *pList = GetCommandList(pPass);

    m_pContext->BeginCommandList(pList);

    /// PREPARE ATTACHMENTS ///
    eastl::span<ColorAttachment> colorAttachments = pPass->m_ColorAttachments;
    eastl::vector<RenderingColorAttachment> renderingColorAttachments(colorAttachments.size());
    DepthAttachment *pDepthAttachment = &pPass->m_DepthAttachment;

    for (u32 i = 0; i < colorAttachments.size(); i++)
    {
        ColorAttachment &attachment = colorAttachments[i];
        RenderingColorAttachment &renderingAttachment = renderingColorAttachments[i];
        Image *pImage = GetAttachmentImage(attachment);

        ImageLayout currentLayout = pImage->m_Layout;
        ImageLayout nextLayout = ToImageLayout(attachment.m_Access);
        bool attachmentLoad = (attachment.m_Flags & LR_ATTACHMENT_FLAG_LOAD);
        bool attachmentStore = (attachment.m_Access & LR_MEMORY_ACCESS_IMAGE_WRITE_UTL);
        bool attachmentPresent = (attachment.m_Flags & LR_ATTACHMENT_FLAG_PRESENT);

        PipelineBarrier barrier = {
            .m_SrcLayout = currentLayout,
            .m_DstLayout = attachmentPresent ? LR_IMAGE_LAYOUT_PRESENT : nextLayout,
            .m_SrcStage = ToPipelineStage(currentLayout),
            .m_DstStage = ToPipelineStage(nextLayout),
            .m_SrcAccess = ToMemoryAccess(currentLayout, !attachmentLoad),
            .m_DstAccess = attachment.m_Access,
        };
        pList->SetImageBarrier(pImage, &barrier);

        renderingAttachment = {
            .m_pImage = pImage,
            .m_LoadOp = attachmentLoad ? LR_ATTACHMENT_OP_LOAD : LR_ATTACHMENT_OP_DONT_CARE,
            .m_StoreOp = attachmentStore ? LR_ATTACHMENT_OP_STORE : LR_ATTACHMENT_OP_DONT_CARE,
        };
    }

    if (pPass->m_Flags & LR_RENDER_PASS_FLAG_ALLOW_EXECUTE)
    {
        RenderingBeginDesc renderingDesc = {
            .m_RenderArea = { 0, 0, pSwapChain->m_Width, pSwapChain->m_Height },
            .m_ColorAttachments = renderingColorAttachments,
        };

        pList->BeginRendering(&renderingDesc);
        pList->SetPipeline(pPass->m_pPipeline);
        pPass->Execute(m_pContext, pList);
        pList->EndRendering();
    }

    m_pContext->EndCommandList(pList);

    CommandListSubmitDesc pListSubmitDesc[] = { pList };
    SemaphoreSubmitDesc pSignalSemaDesc[1] = {};

    SubmitDesc submitDesc = {};
    submitDesc.m_Type = pPass->m_PassType;
    submitDesc.m_Lists = pListSubmitDesc;

    if (pPass->m_NameHash == FNV64HashOf("$present"))
    {
        Semaphore *pSemaphore = GetSemaphore(LR_COMMAND_LIST_TYPE_GRAPHICS);
        pSignalSemaDesc[0] = { pSemaphore, ++pSemaphore->m_Value, LR_PIPELINE_STAGE_RENDER_TARGET };

        submitDesc.m_SignalSemas = pSignalSemaDesc;
    }

    m_pContext->Submit(&submitDesc);
}

RenderPassGroup *RenderGraph::GetGroup(u32 groupID)
{
    ZoneScoped;

    return m_Groups[groupID];
}

RenderPassGroup *RenderGraph::GetGroup(eastl::string_view name)
{
    ZoneScoped;

    Hash64 hash = Hash::FNV64String(name);
    for (RenderPassGroup *pGroup : m_Groups)
        if (pGroup->m_Hash == hash)
            return pGroup;

    return nullptr;
}

RenderPassGroup *RenderGraph::GetOrCreateGroup(eastl::string_view name)
{
    ZoneScoped;

    Hash64 hash = Hash::FNV64String(name);
    for (RenderPassGroup *pGroup : m_Groups)
        if (pGroup->m_Hash == hash)
            return pGroup;

    Memory::AllocationInfo info = {
        .m_Size = sizeof(RenderPassGroup),
        .m_Alignment = 8,
    };
    m_GroupAllocator.Allocate(info);

    RenderPassGroup *pGroup = new (info.m_pData) RenderPassGroup;
    pGroup->m_ID = m_Groups.size();
    pGroup->m_Hash = hash;

    m_Groups.push_back(pGroup);

    return pGroup;
}

void RenderGraph::AllocateCommandLists(CommandListType type)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        CommandAllocator *pAllocator = m_pCommandAllocators[i];
        CommandList *pList = m_pContext->CreateCommandList(type, pAllocator);
        m_pContext->SetObjectName(pList, Format("List {}", m_CommandLists.size()));
        m_CommandLists.push_back(pList);
    }
}

Semaphore *RenderGraph::GetSemaphore(CommandListType type)
{
    ZoneScoped;

    u32 frameIdx = m_pContext->GetSwapChain()->m_CurrentFrame;
    return m_pSemaphores[frameIdx];
}

CommandAllocator *RenderGraph::GetCommandAllocator(CommandListType type)
{
    ZoneScoped;

    u32 frameIdx = m_pContext->GetSwapChain()->m_CurrentFrame;
    return m_pCommandAllocators[frameIdx];
}

CommandList *RenderGraph::GetCommandList(RenderPass *pPass)
{
    ZoneScoped;

    VKSwapChain *pSwapChain = m_pContext->GetSwapChain();
    return m_CommandLists[pPass->m_PassID * pSwapChain->m_FrameCount + pSwapChain->m_CurrentFrame];
}

Image *RenderGraph::GetAttachmentImage(const ColorAttachment &attachment)
{
    ZoneScoped;

    if (attachment.m_Hash == FNV64HashOf("$backbuffer"))
        return m_pContext->GetSwapChain()->GetCurrentFrame()->m_pImage;

    for (auto &pair : m_Images)
        if (pair.first == attachment.m_Hash)
            return pair.second;

    return nullptr;
}
}  // namespace lr::Graphics