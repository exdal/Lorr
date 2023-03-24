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
struct RenderPassGroup
{
    u32 m_ID = ~0;
    u64 m_Hash = ~0;
    u64 m_DependencyMask = 0;
    eastl::fixed_vector<RenderPass *, LR_RG_MAX_RENDERPASS_PER_GROUP_COUNT, false> m_Passes;
};

struct RenderGraphDesc
{
    APIDesc &m_APIDesc;
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

        Hash64 hash = Hash::FNV64String(name);
        for (RenderPass *pPass : m_Passes)
            if (pPass->m_NameHash == hash)
                return (_RenderPass *)pPass;

        return nullptr;
    }

    template<typename _Data, typename... _Args>
    GraphicsRenderPass *CreateGraphicsPass(eastl::string_view name, _Args &&...args)
    {
        ZoneScoped;

        Memory::AllocationInfo info = {
            .m_Size = sizeof(GraphicsRenderPassCallback<_Data>),
            .m_Alignment = 8,
        };
        m_PassAllocator.Allocate(info);

        GraphicsRenderPass *pPass = new (info.m_pData)
            GraphicsRenderPassCallback<_Data>(name, eastl::forward<_Args>(args)..., m_pContext);

        pPass->m_PassID = m_Passes.size();
        m_Passes.push_back(pPass);

        AllocateCommandLists(LR_COMMAND_LIST_TYPE_GRAPHICS);

        return pPass;
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
    Image *GetAttachmentImage(const ColorAttachment &attachment);

    /// API CONTEXT ///
    VKContext *m_pContext = nullptr;
    Semaphore *m_pSemaphores[LR_MAX_FRAME_COUNT] = {};
    CommandAllocator *m_pCommandAllocators[LR_MAX_FRAME_COUNT] = {};
    eastl::vector<CommandList *> m_CommandLists;

    /// RESOURCES ///
    Memory::LinearAllocator m_PassAllocator = {};
    Memory::LinearAllocator m_GroupAllocator = {};
    eastl::vector<RenderPass *> m_Passes;
    eastl::vector<RenderPassGroup *> m_Groups;
    eastl::vector<eastl::pair<Hash64, Image *>> m_Images;
};
}  // namespace lr::Graphics