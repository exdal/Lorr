//
// Created on Friday 24th February 2023 by exdal
//

#pragma once

#include "RenderGraphResource.hh"

#define LR_INVALID_GROUP_ID ~0

namespace lr::Graphics
{
enum RenderPassFlags : u32
{
    LR_RENDER_PASS_FLAG_NONE = 0,
    LR_RENDER_PASS_FLAG_ALLOW_PREPARE = 1 << 0,
    LR_RENDER_PASS_FLAG_ALLOW_EXECUTE = 1 << 1,
    LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN = 1 << 2,
};
EnumFlags(RenderPassFlags);

struct RenderPass
{
    Hash64 m_NameHash = ~0;
    RenderPassFlags m_Flags = LR_RENDER_PASS_FLAG_NONE;

    CommandListType m_PassType : 3 = LR_COMMAND_LIST_TYPE_GRAPHICS;
    u32 m_BoundGroupID : 13 = LR_INVALID_GROUP_ID;
    u32 m_PassID : 16 = 0;

    Pipeline *m_pPipeline = nullptr;
    RenderGraphResourceManager *m_pResourceMan = nullptr;

    virtual void Prepare(CommandList *pList) = 0;
    virtual void Execute(CommandList *pList) = 0;
    virtual void Shutdown() = 0;
};

struct GraphicsRenderPass : RenderPass
{
    eastl::vector<ColorAttachment> m_ColorAttachments;
    DepthAttachment m_DepthAttachment = {};
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(RenderGraphResourceManager &, _Data &)>;
template<typename _Data>
using RenderPassPrepareFn = eastl::function<void(RenderGraphResourceManager &, _Data &, CommandList *)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(RenderGraphResourceManager &, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(RenderGraphResourceManager &)>;

template<typename _Data>
struct GraphicsRenderPassCallback : GraphicsRenderPass, _Data
{
    GraphicsRenderPassCallback(eastl::string_view name)
    {
        ZoneScoped;

        m_NameHash = Hash::FNV64String(name);
        m_PassType = LR_COMMAND_LIST_TYPE_GRAPHICS;
    }

    void SetCallbacks(
        const RenderPassSetupFn<_Data> &fSetup,
        const RenderPassExecuteFn<_Data> &fExecute,
        const RenderPassShutdownFn &fShutdown)
    {
        ZoneScoped;

        m_fExecute = fExecute;
        m_fShutdown = fShutdown;
        m_Flags |= LR_RENDER_PASS_FLAG_ALLOW_EXECUTE | LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN;

        fSetup(*m_pResourceMan, (_Data &)(*this));
    }

    void SetCallbacks(
        const RenderPassSetupFn<_Data> &fSetup,
        const RenderPassPrepareFn<_Data> &fPrepare,
        const RenderPassExecuteFn<_Data> &fExecute,
        const RenderPassShutdownFn &fShutdown)
    {
        ZoneScoped;

        m_fPrepare = fPrepare;
        m_fExecute = fExecute;
        m_fShutdown = fShutdown;
        m_Flags |= LR_RENDER_PASS_FLAG_ALLOW_PREPARE | LR_RENDER_PASS_FLAG_ALLOW_EXECUTE
                   | LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN;

        fSetup(*m_pResourceMan, (_Data &)(*this));
    }

    void Prepare(CommandList *pList) override
    {
        ZoneScoped;

        m_fPrepare(*m_pResourceMan, (_Data &)(*this), pList);
    }

    void Execute(CommandList *pList) override
    {
        ZoneScoped;

        m_fExecute(*m_pResourceMan, (_Data &)(*this), pList);
    }

    void Shutdown() override
    {
        ZoneScoped;

        m_fShutdown(*m_pResourceMan);
    }

    RenderPassPrepareFn<_Data> m_fPrepare = nullptr;
    RenderPassExecuteFn<_Data> m_fExecute = nullptr;
    RenderPassShutdownFn m_fShutdown = nullptr;
};

}  // namespace lr::Graphics