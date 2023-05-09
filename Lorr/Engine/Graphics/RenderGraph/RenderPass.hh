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
    ResourceView(Hash64 hash, _Resource *pResource)
        : m_Hash(hash),
          m_pHandle(pResource)
    {
    }
    ResourceView(NameID name, _Resource *pResource)
        : m_Hash(Hash::FNV64String(name)),
          m_pHandle(pResource)
    {
    }
    Hash64 &Hash() { return m_Hash; }
    _Resource *&Resource() { return m_pHandle; }

    Hash64 m_Hash;
    _Resource *m_pHandle;
};

enum InputResourceFlag : u32
{
    None = 0,
    SwapChainScaling = 1 << 0,  // Attachment's size is relative to swapchain size
    SwapChainFormat = 1 << 1,   // Attachment's format is relative to swapchain format
    Present = 1 << 2,
    ColorAttachment = 1 << 3,
    DepthAttachment = 1 << 4,
};
EnumFlags(InputResourceFlag);

struct InputAttachmentOp
{
    AttachmentOp m_LoadOp;
    AttachmentOp m_Store;
};

struct InputResourceAccess
{
    MemoryAccess m_SrcAccess;
    MemoryAccess m_DstAccess;
};

struct RenderPassInput
{
    Hash64 m_Hash = ~0;
    InputResourceFlag m_Flags = InputResourceFlag::None;

    union
    {
        u64 __u_data = 0;

        /// For Render Attachments
        InputAttachmentOp m_AttachmentOp;

        /// For Input Resources
        InputResourceAccess m_Access;
    };

    union
    {
        ColorClearValue m_ColorClearVal = {};
        DepthClearValue m_DepthClearVal;
    };
};

enum class RenderPassFlag : u32
{
    None = 0,
    AllowExecute = 1 << 0,
    AllowShutdown = 1 << 1,
    AllowSetup = 1 << 2,
    SkipRendering = 1 << 3,
};
EnumFlags(RenderPassFlag);

struct RenderPassBuilder;
struct RenderPass
{
    virtual void Setup(Context *pContext, RenderPassBuilder *pBuilder) = 0;
    virtual void Execute(Context *pContext, CommandList *pList) = 0;
    virtual void Shutdown(Context *pContext) = 0;

    Hash64 m_NameHash = ~0;
    RenderPassFlag m_Flags = RenderPassFlag::None;
    CommandType m_PassType = CommandType::Count;

    /// GROUP METADATA ///
    u32 m_SubmitID = 0;
    u32 m_PassID = 0;
    u32 m_ResourceIndex = 0;
    u32 m_ResourceCount = 0;

    Pipeline *m_pPipeline = nullptr;
};

struct GraphicsRenderPass : RenderPass
{
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(Context *, _Data &, RenderPassBuilder &)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(Context *, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(Context *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : GraphicsRenderPass, _Data
{
    void Setup(Context *pContext, RenderPassBuilder *pBuilder) override
    {
        ZoneScoped;

        m_fSetup(pContext, (_Data &)(*this), (*pBuilder));
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

struct RenderGraph;
struct RenderPassBuilder
{
    RenderPassBuilder(RenderGraph *pGraph);
    ~RenderPassBuilder();

    void BuildPass(RenderPass *pPass);
    void SetupGraphicsPass();

    void SetColorAttachment(
        NameID resource, InputAttachmentOp attachmentOp, InputResourceFlag flags = InputResourceFlag::None);
    void SetInputResource(
        NameID resource, InputResourceAccess access, InputResourceFlag flags = InputResourceFlag::None);
    void SetBlendAttachment(const ColorBlendAttachment &attachment);
    void SetPushConstant(const PushConstantDesc &pushConstant);
    void SetResourceDescriptorSet(DescriptorSetLayout *pLayout, eastl::span<DescriptorGetInfo> elements);
    void SetSamplerDescriptorSet(DescriptorSetLayout *pLayout, eastl::span<DescriptorGetInfo> elements);
    void SetDescriptorSetLayout(DescriptorSetLayout *pLayout);
    void SetShader(Shader *pShader);
    void SetInputLayout(const InputLayout &layout);

    Buffer *GetResourceDescriptorBuffer() { return m_ResourceDescriptorInfo.m_pBuffer; }
    u64 GetResourceDescriptorOffset() { return m_ResourceDescriptorInfo.m_BufferOffset; }
    Buffer *GetSamplerDescriptorBuffer() { return m_SamplerDescriptorInfo.m_pBuffer; }
    u64 GetSamplerDescriptorOffset() { return m_SamplerDescriptorInfo.m_BufferOffset; }

    union
    {
        RenderPass *m_pRenderPass = nullptr;
        GraphicsRenderPass *m_pGraphicsPass;
    };

    GraphicsPipelineBuildInfo m_GraphicsPipelineInfo = {};
    ComputePipelineBuildInfo m_ComputePipelineInfo = {};

    struct DescriptorBufferInfo
    {
        Buffer *m_pBuffer = nullptr;
        u64 m_BufferOffset = 0;
    };

    DescriptorBufferInfo m_ResourceDescriptorInfo = {};
    DescriptorBufferInfo m_SamplerDescriptorInfo = {};

    Context *m_pContext = nullptr;
    RenderGraph *m_pGraph = nullptr;
};

}  // namespace lr::Graphics