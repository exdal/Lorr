//
// Created on Friday 24th February 2023 by exdal
//

#pragma once

#include "Core/Crypt/FNV.hh"
#include "Core/Graphics/Vulkan/VKContext.hh"

#define LR_INVALID_GROUP_ID ~0

namespace lr::Graphics
{
using Hash64 = u64;
using NameID = eastl::string_view;

enum AttachmentFlags : u32
{
    LR_ATTACHMENT_FLAG_NONE = 0,
    LR_ATTACHMENT_FLAG_SC_SCALING = 1 << 0,  // Attachment's size is relative to swapchain size
    LR_ATTACHMENT_FLAG_SC_FORMAT = 1 << 1,   // Attachment's format is relative to swapchain format
    LR_ATTACHMENT_FLAG_LOAD = 1 << 2,
    LR_ATTACHMENT_FLAG_CLEAR = 1 << 3,
    LR_ATTACHMENT_FLAG_PRESENT = 1 << 4,
};
EnumFlags(AttachmentFlags);

struct ColorAttachment
{
    Hash64 m_Hash = ~0;
    MemoryAccess m_Access = LR_MEMORY_ACCESS_NONE;
    AttachmentFlags m_Flags = LR_ATTACHMENT_FLAG_NONE;
};

struct DepthAttachment
{
    Hash64 m_Hash = ~0;
};

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

    virtual void Prepare(VKContext *pContext, CommandList *pList) = 0;
    virtual void Execute(VKContext *pContext, CommandList *pList) = 0;
    virtual void Shutdown(VKContext *pContext) = 0;
};

struct GraphicsRenderPass : RenderPass
{
    void AddColorAttachment(NameID name, MemoryAccess access, AttachmentFlags flags = LR_ATTACHMENT_FLAG_LOAD);

    eastl::vector<ColorAttachment> m_ColorAttachments;
    DepthAttachment m_DepthAttachment = {};
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(VKContext *, _Data &)>;
template<typename _Data>
using RenderPassPrepareFn = eastl::function<void(VKContext *, _Data &, CommandList *)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(VKContext *, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(VKContext *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : GraphicsRenderPass, _Data
{
    GraphicsRenderPassCallback(eastl::string_view name, VKContext *pContext)
    {
        ZoneScoped;

        m_NameHash = Hash::FNV64String(name);
        m_PassType = LR_COMMAND_LIST_TYPE_GRAPHICS;
        m_Flags = LR_RENDER_PASS_FLAG_NONE;
    }

    GraphicsRenderPassCallback(
        eastl::string_view name,
        const RenderPassSetupFn<_Data> &fSetup,
        const RenderPassExecuteFn<_Data> &fExecute,
        const RenderPassShutdownFn &fShutdown,
        VKContext *pContext)
    {
        ZoneScoped;

        m_NameHash = Hash::FNV64String(name);
        m_PassType = LR_COMMAND_LIST_TYPE_GRAPHICS;
        m_fExecute = fExecute;
        m_fShutdown = fShutdown;
        m_Flags |= LR_RENDER_PASS_FLAG_ALLOW_EXECUTE | LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN;

        fSetup(pContext, (_Data &)(*this));
    }

    GraphicsRenderPassCallback(
        eastl::string_view name,
        const RenderPassSetupFn<_Data> &fSetup,
        const RenderPassPrepareFn<_Data> &fPrepare,
        const RenderPassExecuteFn<_Data> &fExecute,
        const RenderPassShutdownFn &fShutdown,
        VKContext *pContext)
    {
        ZoneScoped;

        m_NameHash = Hash::FNV64String(name);
        m_PassType = LR_COMMAND_LIST_TYPE_GRAPHICS;
        m_fPrepare = fPrepare;
        m_fExecute = fExecute;
        m_fShutdown = fShutdown;
        m_Flags |= LR_RENDER_PASS_FLAG_ALLOW_PREPARE | LR_RENDER_PASS_FLAG_ALLOW_EXECUTE
                   | LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN;

        fSetup(pContext, (_Data &)(*this));
    }

    void Prepare(VKContext *pContext, CommandList *pList) override
    {
        ZoneScoped;

        m_fPrepare(pContext, (_Data &)(*this), pList);
    }

    void Execute(VKContext *pContext, CommandList *pList) override
    {
        ZoneScoped;

        m_fExecute(pContext, (_Data &)(*this), pList);
    }

    void Shutdown(VKContext *pContext) override
    {
        ZoneScoped;

        m_fShutdown(pContext);
    }

    RenderPassPrepareFn<_Data> m_fPrepare = nullptr;
    RenderPassExecuteFn<_Data> m_fExecute = nullptr;
    RenderPassShutdownFn m_fShutdown = nullptr;
};

}  // namespace lr::Graphics