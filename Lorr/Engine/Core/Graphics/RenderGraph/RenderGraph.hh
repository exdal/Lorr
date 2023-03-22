//
// Created on Friday 24th February 2023 by exdal
//

#pragma once

#include "RenderGraphResource.hh"
#include "RenderPass.hh"

#define LR_RG_MAX_GROUP_COUNT 64
#define LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT 8
#define LR_RG_MAX_RENDERPASS_COUNT LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT *LR_RG_MAX_GROUP_COUNT

namespace lr::Graphics
{
struct RenderPassGroup
{
    u32 m_ID = ~0;
    u64 m_Hash = ~0;
    u64 m_DependencyMask = 0;
    eastl::fixed_vector<RenderPass *, LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT, false> m_Passes;
};

struct RenderGraphDesc
{
    VKAPI *m_pAPI = nullptr;
};

struct RenderGraph
{
    void Init(RenderGraphDesc *pDesc);
    void Shutdown();
    void Draw();

    /// RENDERPASSES ///

    template<typename _RenderPass = RenderPass>
    _RenderPass *GetPass(eastl::string_view name)
    {
        ZoneScoped;

        ResourceID hash = Hash::FNV64String(name);
        for (RenderPass *pPass : m_Passes)
            if (pPass->m_NameHash == hash)
                return (_RenderPass *)pPass;

        return nullptr;
    }

    template<typename _Data, typename... _Args>
    GraphicsRenderPass *CreateGraphicsPass(eastl::string_view name)
    {
        ZoneScoped;

        Memory::AllocationInfo info = {
            .m_Size = sizeof(GraphicsRenderPassCallback<_Data>),
            .m_Alignment = 8,
        };
        m_PassAllocator.Allocate(info);

        GraphicsRenderPass *pPass = new (info.m_pData) GraphicsRenderPassCallback<_Data>(name);
        pPass->m_pResourceMan = &m_ResourceManager;
        pPass->m_PassID = m_Passes.size();
        m_Passes.push_back(pPass);

        AllocateCommandLists(LR_COMMAND_LIST_TYPE_GRAPHICS);

        return pPass;
    }

    template<typename _Data, typename... _Args>
    void SetCallbacks(GraphicsRenderPass *pPass, _Args &&...args)
    {
        GraphicsRenderPassCallback<_Data> *pCallbackPass = (GraphicsRenderPassCallback<_Data> *)pPass;
        pCallbackPass->SetCallbacks(eastl::forward<_Args>(args)..., m_ResourceManager);
    }

    void ExecuteGraphics(GraphicsRenderPass *pPass);

    /// GROUPS ///

    void AddGraphicsGroup(
        eastl::string_view name, eastl::string_view dependantGroup, cinitl<eastl::string_view> &passes);

    RenderPassGroup *GetGroup(u32 groupID);
    RenderPassGroup *GetGroup(eastl::string_view name);
    RenderPassGroup *GetOrCreateGroup(eastl::string_view name);

    /// RESOURCES ///

    void AllocateCommandLists(CommandListType type);
    Semaphore *GetSemaphore(CommandListType type);
    CommandAllocator *GetCommandAllocator(CommandListType type);
    CommandList *GetCommandList(RenderPass *pPass);

    /// API CONTEXT ///
    VKAPI *m_pAPI = nullptr;

    /// RESOURCES ///
    Semaphore *m_pSemaphores[LR_MAX_FRAME_COUNT] = {};
    CommandAllocator *m_pCommandAllocators[LR_MAX_FRAME_COUNT] = {};
    eastl::vector<CommandList *> m_CommandLists;

    Memory::LinearAllocator m_PassAllocator = {};
    Memory::LinearAllocator m_GroupAllocator = {};
    eastl::vector<RenderPass *> m_Passes;
    eastl::vector<RenderPassGroup *> m_Groups;
    eastl::vector<TrackedResource> m_Resources;
};

}  // namespace lr::Graphics