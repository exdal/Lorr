//
// Created on Friday 24th February 2023 by exdal
//

#pragma once

#include "RenderPass.hh"

#define LR_RG_MAX_GROUP_COUNT 64
#define LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT 8
#define LR_RG_MAX_RENDERPASS_COUNT LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT *LR_RG_MAX_GROUP_COUNT

namespace lr::Graphics
{
template<typename _Resource, typename _Desc>
struct ResourceCache
{
    _Resource *Get(eastl::span<_Desc> elements, u64 &hashOut)
    {
        ZoneScoped;

        hashOut = Hash::FNV64((char *)elements.data(), elements.size_bytes());
        for (auto &[hash, pHandle] : m_Resources)
            if (hashOut == hash)
                return pHandle;

        return nullptr;
    }

    void Add(_Resource *pResource, u64 hash)
    {
        ZoneScoped;

        m_Resources.push_back({ hash, pResource });
    }

    eastl::vector<ResourceView<_Resource>> m_Resources = {};
};

struct RenderGraphDesc
{
    APIDesc &m_APIDesc;
};

struct RenderGraph
{
    void Init(RenderGraphDesc *pDesc);
    void Shutdown();
    void Prepare();
    void Draw();

    /// RENDERPASSES ///

    template<typename _Pass>
    _Pass *AllocateRenderPassCallback()
    {
        Memory::AllocationInfo info = {
            .m_Size = sizeof(_Pass),
            .m_Alignment = 8,
        };
        m_PassAllocator.Allocate(info);

        return new (info.m_pData) _Pass;
    }

    template<typename _Data>
    GraphicsRenderPass *CreateGraphicsPass(
        eastl::string_view name, RenderPassFlags flags = LR_RENDER_PASS_FLAG_NONE)
    {
        ZoneScoped;

        auto *pPass = AllocateRenderPassCallback<GraphicsRenderPassCallback<_Data>>();

        pPass->m_NameHash = Hash::FNV64String(name);
        pPass->m_Flags = flags;
        pPass->m_PassType = LR_COMMAND_LIST_TYPE_GRAPHICS;
        pPass->m_PassID = m_Passes.size();

        m_Passes.push_back(pPass);

        return pPass;
    }

    template<typename _Data>
    GraphicsRenderPass *CreateGraphicsPassCb(
        eastl::string_view name,
        const RenderPassSetupFn<_Data> &fSetup,
        const RenderPassExecuteFn<_Data> &fExecute,
        const RenderPassShutdownFn &fShutdown)
    {
        ZoneScoped;

        auto *pPass = (GraphicsRenderPassCallback<_Data> *)CreateGraphicsPass<_Data>(name);

        pPass->m_Flags |= LR_RENDER_PASS_FLAG_ALLOW_SETUP | LR_RENDER_PASS_FLAG_ALLOW_EXECUTE
                          | LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN;
        pPass->m_fSetup = fSetup;
        pPass->m_fExecute = fExecute;
        pPass->m_fShutdown = fShutdown;

        return pPass;
    }

    template<typename _RenderPass = RenderPass>
    _RenderPass *GetPass(eastl::string_view name)
    {
        ZoneScoped;

        Hash64 hash = Hash::FNV64String(name);
        for (RenderPass *pPass : m_Passes)
            if (pPass->m_NameHash == hash)
                return (_RenderPass *)pPass;

        return nullptr;
    }

    /// RESOURCES ///

    Image *GetAttachmentImage(const RenderPassAttachment &attachment);
    Image *CreateImage(NameID name, ImageDesc &desc, MemoryAccess initialAccess = LR_MEMORY_ACCESS_NONE);
    Image *CreateImage(NameID name, Image *pImage, MemoryAccess initialAccess = LR_MEMORY_ACCESS_NONE);

    //! THIS PERFORMS CPU WAIT!!!
    void SubmitList(CommandList *pList, PipelineStage waitStage, PipelineStage signalStage);
    void SetImageBarrier(CommandList *pList, Image *pImage, MemoryAccess srcAccess, MemoryAccess dstAccess);
    void UploadImageData(Image *pImage, MemoryAccess resultAccess, void *pData, u32 dataSize);

    void BuildPassBarriers(RenderPass *pPass);
    void FinalizeGraphicsPass(GraphicsRenderPass *pPass, GraphicsPipelineBuildInfo *pPipelineInfo);
    void RecordGraphicsPass(GraphicsRenderPass *pPass, CommandList *pList);

    void SetMemoryAccess(Hash64 resource, MemoryAccess access);
    MemoryAccess GetResourceAccess(Hash64 resource);

    /// SUBMISSION ///

    void AllocateCommandLists(CommandListType type);

    Semaphore *GetSemaphore();
    CommandAllocator *GetCommandAllocator(CommandListType type);
    CommandList *GetCommandList(u32 submitID);
    CommandList *GetCommandList(RenderPass *&pPass) { return GetCommandList(pPass->m_SubmitID); }

    /// API CONTEXT ///

    VKContext *m_pContext = nullptr;
    eastl::vector<Semaphore *> m_Semaphores;
    eastl::vector<CommandAllocator *> m_CommandAllocators;
    eastl::vector<CommandList *> m_CommandLists;
    CommandList *m_pSetupList = nullptr;

    /// RESOURCES ///

    Memory::LinearAllocator m_PassAllocator = {};
    eastl::vector<RenderPass *> m_Passes;

    eastl::vector<ResourceView<Image>> m_Images;
    eastl::vector<ImageBarrier> m_Barriers;
    eastl::vector<MemoryAccess> m_LastAccessInfos;  // Latest access info of each image
};
}  // namespace lr::Graphics