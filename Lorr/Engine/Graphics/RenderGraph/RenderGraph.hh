// Created on Friday February 24th 2023 by exdal
// Last modified on Thursday May 25th 2023 by exdal

#pragma once

#include "RenderPass.hh"

namespace lr::Graphics
{
struct PipelineAccessInfo
{
    MemoryAccess m_Access = MemoryAccess::None;
    PipelineStage m_Stage = PipelineStage::None;
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
        void *pData = m_PassAllocator.Allocate(sizeof(_Pass));
        return new (pData) _Pass;
    }

    template<typename _Data>
    GraphicsRenderPass *CreateGraphicsPass(eastl::string_view name, RenderPassFlag flags = RenderPassFlag::None)
    {
        ZoneScoped;

        auto *pPass = AllocateRenderPassCallback<GraphicsRenderPassCallback<_Data>>();

        pPass->m_NameHash = Hash::FNV64String(name);
        pPass->m_Flags = flags;
        pPass->m_PassType = CommandType::Graphics;
        pPass->m_PassID = m_Passes.size();

        m_Passes.push_back(pPass);

        return pPass;
    }

    template<typename _Data>
    GraphicsRenderPass *CreateGraphicsPassCb(eastl::string_view name, const RenderPassSetupFn<_Data> &fSetup)
    {
        ZoneScoped;

        auto *pPass = (GraphicsRenderPassCallback<_Data> *)CreateGraphicsPass<_Data>(name);

        pPass->m_Flags |= RenderPassFlag::AllowSetup;
        pPass->m_fSetup = fSetup;

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

        pPass->m_Flags |=
            RenderPassFlag::AllowSetup | RenderPassFlag::AllowExecute | RenderPassFlag::AllowShutdown;
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

    Image *GetInputImage(const RenderPassInput &input);
    Image *CreateImage(NameID name, ImageDesc &desc, MemoryAccess initialAccess = MemoryAccess::None);
    Image *CreateImage(NameID name, Image *pImage, MemoryAccess initialAccess = MemoryAccess::None);

    //! THIS PERFORMS CPU WAIT!!!
    void SubmitList(CommandList *pList, PipelineStage waitStage, PipelineStage signalStage);
    void SetImageBarrier(CommandList *pList, Image *pImage, ImageBarrier &barrier);

    void RecordGraphicsPass(GraphicsRenderPass *pPass, CommandList *pList);

    /// SUBMISSION ///

    void AllocateCommandLists(CommandType type);

    Semaphore *GetSemaphore();
    CommandAllocator *GetCommandAllocator(CommandType type);
    CommandList *GetCommandList(u32 submitID);
    CommandList *GetCommandList(RenderPass *&pPass) { return GetCommandList(pPass->m_SubmitID); }

    /// API CONTEXT ///

    Context *m_pContext = nullptr;
    eastl::vector<Semaphore *> m_Semaphores;
    eastl::vector<CommandAllocator *> m_CommandAllocators;
    eastl::vector<CommandList *> m_CommandLists;
    CommandList *m_pSetupList = nullptr;

    /// RESOURCES ///

    eastl::span<RenderPassInput> GetPassResources(RenderPass *pPass)
    {
        return eastl::span(m_InputResources.begin() + pPass->m_ResourceIndex, pPass->m_ResourceCount);
    }

    Memory::LinearAllocator m_PassAllocator = {};
    eastl::vector<RenderPass *> m_Passes;

    eastl::vector<ResourceView<Image>> m_Images;
    eastl::vector<RenderPassInput> m_InputResources;

    Buffer *m_pResourceDescriptorBuffer = nullptr;
    Buffer *m_pSamplerDescriptorBuffer = nullptr;
};

}  // namespace lr::Graphics