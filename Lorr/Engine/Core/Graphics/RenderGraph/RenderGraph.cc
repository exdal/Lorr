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

    m_ResourceManager.m_pAPI = pDesc->m_pAPI;
    m_ResourceManager.m_Resources.reserve(Memory::KiBToBytes(16));
    m_pAPI = pDesc->m_pAPI;

    for (u32 i = 0; i < LR_MAX_FRAME_COUNT; i++)
    {
        m_pCommandAllocators[i] = m_pAPI->CreateCommandAllocator(LR_COMMAND_LIST_TYPE_GRAPHICS, true);
        m_pAPI->SetObjectName(m_pCommandAllocators[i], Format("Render Graph Command Allocator {}", i));
        m_pSemaphores[i] = m_pAPI->CreateSemaphore(0, false);
        m_pAPI->SetObjectName(m_pSemaphores[i], Format("Render Graph Semaphore {}", i));
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

    SwapChain *pSwapChain = m_pAPI->GetSwapChain();
    SwapChainFrame *pCurrentFrame = m_pAPI->GetCurrentFrame();

    Semaphore *pCurrentSemp = m_pSemaphores[pSwapChain->m_CurrentFrame];
    CommandAllocator *pCurrentAllocator = m_pCommandAllocators[pSwapChain->m_CurrentFrame];

    m_pAPI->WaitForSemaphore(pCurrentSemp, pCurrentSemp->m_Value);
    m_pAPI->ResetCommandAllocator(pCurrentAllocator);

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

static ImageLayout ToImageLayout(MemoryAcces access)
{
    ZoneScoped;

    switch (usage)
    {
        case LR_IMAGE_USAGE_RENDER_TARGET:
            return LR_IMAGE_LAYOUT_ATTACHMENT;
        case LR_IMAGE_USAGE_DEPTH_STENCIL:
        case LR_IMAGE_USAGE_SHADER_RESOURCE:
            return LR_IMAGE_LAYOUT_READ_ONLY;
        case LR_IMAGE_USAGE_TRANSFER_SRC:
            return LR_IMAGE_LAYOUT_TRANSFER_SRC;
        case LR_IMAGE_USAGE_TRANSFER_DST:
            return LR_IMAGE_LAYOUT_TRANSFER_DST;
        case LR_IMAGE_USAGE_UNORDERED_ACCESS:
            return LR_IMAGE_LAYOUT_UNORDERED_ACCESS;
        case LR_IMAGE_USAGE_PRESENT:
            return LR_IMAGE_LAYOUT_PRESENT;
        default:
            LOG_ERROR("One ImageUsage flag must be used in render passes.");
            return LR_IMAGE_LAYOUT_UNDEFINED;
    }
}

static PipelineStage ToPipelineStage(ImageLayout layout)
{
    ZoneScoped;

    constexpr static PipelineStage kColorStages[] = {
        LR_PIPELINE_STAGE_NONE,            // LR_IMAGE_LAYOUT_UNDEFINED
        LR_PIPELINE_STAGE_NONE,            // LR_IMAGE_LAYOUT_PRESENT
        LR_PIPELINE_STAGE_RENDER_TARGET,   // LR_IMAGE_LAYOUT_ATTACHMENT
        LR_PIPELINE_STAGE_PIXEL_SHADER,    // LR_IMAGE_LAYOUT_READ_ONLY
        LR_PIPELINE_STAGE_TRANSFER,        // LR_IMAGE_LAYOUT_TRANSFER_SRC | LR_IMAGE_LAYOUT_TRANSFER_DST
        LR_PIPELINE_STAGE_COMPUTE_SHADER,  // LR_IMAGE_LAYOUT_UNORDERED_ACCESS
    };

    return kColorStages[layout];
}

static MemoryAcces ToCurrentMemoryAccess(ImageLayout layout, AttachmentFlags flags)
{
    ZoneScoped;

    bool isWrite = flags != LR_ATTACHMENT_FLAG_CLEAR;  // all other flags are WRITE op.

    struct AccessPair
    {
        MemoryAcces m_Read;
        MemoryAcces m_Write;
    };
    constexpr static AccessPair kColorAccess[] = {
        { LR_MEMORY_ACCESS_NONE, LR_MEMORY_ACCESS_NONE },                               // UNDEFINED
        { LR_MEMORY_ACCESS_NONE, LR_MEMORY_ACCESS_NONE },                               // PRESENT
        { LR_MEMORY_ACCESS_RENDER_TARGET_READ, LR_MEMORY_ACCESS_RENDER_TARGET_WRITE },  // ATTACHMENT
        { LR_MEMORY_ACCESS_SHADER_READ, LR_MEMORY_ACCESS_SHADER_WRITE },                // READ_ONLY
        { LR_MEMORY_ACCESS_TRANSFER_READ, LR_MEMORY_ACCESS_TRANSFER_WRITE },            // TRANSFER_SRC
        { LR_MEMORY_ACCESS_TRANSFER_WRITE, LR_MEMORY_ACCESS_TRANSFER_WRITE },           // TRANSFER_DST
        { LR_MEMORY_ACCESS_MEMORY_READ, LR_MEMORY_ACCESS_MEMORY_WRITE },                // UNORDERED_ACCESS
    };

    const AccessPair &access = kColorAccess[layout];
    return isWrite ? access.m_Write : access.m_Read;
}

void RenderGraph::ExecuteGraphics(GraphicsRenderPass *pPass)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pAPI->GetSwapChain();
    CommandList *pList = GetCommandList(pPass);

    m_pAPI->BeginCommandList(pList);

    /// PREPARE ATTACHMENTS ///
    eastl::span<ColorAttachment> colorAttachments = pPass->GetColorAttachments();
    DepthAttachment *pDepthAttachment = pPass->GetDepthAttachment();

    for (ColorAttachment &attachment : colorAttachments)
    {
        Image *pImage = m_ResourceManager.GetResource<Image>(attachment.m_ResourceID);

        ImageLayout currentLayout = pImage->m_Layout;
        ImageLayout nextLayout = ToImageLayout(attachment.m_AccessFlags);

        PipelineBarrier barrier = {
            .m_CurrentLayout = currentLayout,
            .m_NextLayout = nextLayout,
            .m_CurrentStage = ToPipelineStage(currentLayout),
            .m_NextStage = ToPipelineStage(nextLayout),
            .m_CurrentAccess = ToMemoryAccess(currentLayout, attachment.m_LoadOp),
            .m_NextAccess = attachment.m_AccessFlags,
        };
        pList->SetImageBarrier(pImage, &barrier);
    }

    if (pPass->m_Flags & LR_RENDER_PASS_FLAG_ALLOW_EXECUTE)
    {
        RenderPassBeginDesc passDesc = {};
        passDesc.m_ColorAttachments = colorAttachments;
        passDesc.m_RenderArea = { 0, 0, pSwapChain->m_Width, pSwapChain->m_Height };

        pList->BeginPass(&passDesc);
        pList->SetPipeline(pPass->m_pPipeline);
        pPass->Execute(pList);
        pList->EndPass();
    }

    m_pAPI->EndCommandList(pList);

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

    m_pAPI->Submit(&submitDesc);
}

void RenderGraph::AllocateCommandLists(CommandListType type)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pAPI->GetSwapChain();
    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        CommandAllocator *pAllocator = m_pCommandAllocators[i];
        CommandList *pList = m_pAPI->CreateCommandList(type, pAllocator);
        m_pAPI->SetObjectName(pList, Format("List {}", m_CommandLists.size()));
        m_CommandLists.push_back(pList);
    }
}

Semaphore *RenderGraph::GetSemaphore(CommandListType type)
{
    ZoneScoped;

    u32 frameIdx = m_pAPI->GetSwapChain()->m_CurrentFrame;
    return m_pSemaphores[frameIdx];
}

CommandAllocator *RenderGraph::GetCommandAllocator(CommandListType type)
{
    ZoneScoped;

    u32 frameIdx = m_pAPI->GetSwapChain()->m_CurrentFrame;
    return m_pCommandAllocators[frameIdx];
}

CommandList *RenderGraph::GetCommandList(RenderPass *pPass)
{
    ZoneScoped;

    SwapChain *pSwapChain = m_pAPI->GetSwapChain();

    return m_CommandLists[pPass->m_PassID * pSwapChain->m_FrameCount + pSwapChain->m_CurrentFrame];
}

RenderPassGroup *RenderGraph::GetGroup(u32 groupID)
{
    ZoneScoped;

    return m_Groups[groupID];
}

RenderPassGroup *RenderGraph::GetGroup(eastl::string_view name)
{
    ZoneScoped;

    ResourceID hash = Hash::FNV64String(name);
    for (RenderPassGroup *pGroup : m_Groups)
        if (pGroup->m_Hash == hash)
            return pGroup;

    return nullptr;
}

RenderPassGroup *RenderGraph::GetOrCreateGroup(eastl::string_view name)
{
    ZoneScoped;

    ResourceID hash = Hash::FNV64String(name);
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

}  // namespace lr::Graphics