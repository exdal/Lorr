//
// Created on Friday 24th February 2023 by exdal
//

#pragma once

#include "Crypt/FNV.hh"
#include "Graphics/Context.hh"

#define LR_INVALID_GROUP_ID ~0

namespace lr::Graphics
{
using Hash64 = u64;
using NameID = eastl::string_view;
template<typename _Resource>
struct ResourceView
{
    ResourceView(Hash64 hash, _Resource *pResource) : m_Hash(hash), m_pHandle(pResource) {}
    ResourceView(NameID name, _Resource *pResource) : m_Hash(Hash::FNV64String(name)), m_pHandle(pResource) {}
    Hash64 &Hash() { return m_Hash; }
    _Resource *&Resource() { return m_pHandle; }

    Hash64 m_Hash;
    _Resource *m_pHandle;
};

enum AttachmentFlags : u32
{
    LR_ATTACHMENT_FLAG_NONE = 0,
    LR_ATTACHMENT_FLAG_SC_SCALING = 1 << 0,  // Attachment's size is relative to swapchain size
    LR_ATTACHMENT_FLAG_SC_FORMAT = 1 << 1,   // Attachment's format is relative to swapchain format
    LR_ATTACHMENT_FLAG_CLEAR = 1 << 2,
    LR_ATTACHMENT_FLAG_PRESENT = 1 << 3,
};
EnumFlags(AttachmentFlags);

struct RenderPassAttachment
{
    Hash64 m_Hash = ~0;
    MemoryAccess m_Access = MemoryAccess::None;
    AttachmentFlags m_Flags = LR_ATTACHMENT_FLAG_NONE;

    union
    {
        ColorClearValue m_ColorClearVal = {};
        DepthClearValue m_DepthClearVal;
    };

    bool HasClear() { return m_Flags & LR_ATTACHMENT_FLAG_CLEAR; }
    bool IsWrite() { return m_Access & MemoryAccess::ImageWriteUTL; }
    bool IsPresent() { return m_Flags & LR_ATTACHMENT_FLAG_PRESENT; }
};

enum RenderPassFlags : u32
{
    LR_RENDER_PASS_FLAG_NONE = 0,
    LR_RENDER_PASS_FLAG_ALLOW_EXECUTE = 1 << 0,
    LR_RENDER_PASS_FLAG_ALLOW_SHUTDOWN = 1 << 1,
    LR_RENDER_PASS_FLAG_ALLOW_SETUP = 1 << 2,
    LR_RENDER_PASS_FLAG_SKIP_RENDERING = 1 << 3,
};
EnumFlags(RenderPassFlags);

struct RenderPass
{
    virtual void Setup(Context *pContext) = 0;
    virtual void Execute(Context *pContext, CommandList *pList) = 0;
    virtual void Shutdown(Context *pContext) = 0;

    Hash64 m_NameHash = ~0;
    RenderPassFlags m_Flags = LR_RENDER_PASS_FLAG_NONE;
    CommandType m_PassType = CommandType::Count;

    /// GROUP METADATA ///
    u32 m_BoundGroupID : 16 = LR_INVALID_GROUP_ID;
    u32 m_SubmitID : 16 = 0;
    u32 m_PassID = 0;

    Pipeline *m_pPipeline = nullptr;

    eastl::vector<RenderPassAttachment> m_InputAttachments;
    u32 m_BarrierIndex = 0;
};

struct GraphicsRenderPass : RenderPass
{
    void SetColorAttachment(NameID name, MemoryAccess access, AttachmentFlags flags = LR_ATTACHMENT_FLAG_NONE);
    void SetBlendAttachment(const ColorBlendAttachment &attachment);
    void SetShader(Shader *pShader);
    void SetLayout(DescriptorSetLayout *pLayout);
    void SetPushConstant(const PushConstantDesc &pushConstant);
    void SetInputLayout(const InputLayout &layout);

    void SetPipelineInfo(GraphicsPipelineBuildInfo &info) { m_pPipelineInfo = &info; }
    GraphicsPipelineBuildInfo *m_pPipelineInfo = nullptr;

    eastl::vector<ColorBlendAttachment> m_BlendAttachments;
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(Context *, _Data &, GraphicsRenderPass &)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(Context *, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(Context *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : GraphicsRenderPass, _Data
{
    void Setup(Context *pContext) override
    {
        ZoneScoped;

        m_fSetup(pContext, (_Data &)(*this), (GraphicsRenderPass &)(*this));
    }

    void Execute(Context *pContext, CommandList *pList) override
    {
        ZoneScoped;

        m_fExecute(pContext, (_Data &)(*this), pList);
    }

    void Shutdown(Context *pContext) override
    {
        ZoneScoped;

        m_fShutdown(pContext);
    }

    RenderPassSetupFn<_Data> m_fSetup = nullptr;
    RenderPassExecuteFn<_Data> m_fExecute = nullptr;
    RenderPassShutdownFn m_fShutdown = nullptr;
};

}  // namespace lr::Graphics