// Created on Friday February 24th 2023 by exdal
// Last modified on Monday July 17th 2023 by exdal

#pragma once

#include "Crypt/FNV.hh"
#include "Graphics/APIContext.hh"

namespace lr::Graphics
{
using Hash64 = u64;
using NameID = eastl::string_view;
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

struct RenderPassResource
{
    RenderPassResource *m_pPrev = nullptr;
    RenderPassResource *m_pNext = nullptr;

    Hash64 m_Hash = ~0;
    InputResourceFlag m_Flags = InputResourceFlag::None;

    union
    {
        u64 __u_data = 0;

        /// For Render Attachments
        struct
        {
            AttachmentOp m_LoadOp;
            AttachmentOp m_Store;
        };

        /// For Input Resources
        struct
        {
            MemoryAccess m_SrcAccess;
            MemoryAccess m_DstAccess;
        };
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
    virtual void Setup(APIContext *pContext, RenderPassBuilder *pBuilder) = 0;
    virtual void Execute(APIContext *pContext, CommandList *pList) = 0;
    virtual void Shutdown(APIContext *pContext) = 0;

    RenderPass *m_pPrev = nullptr;
    RenderPass *m_pNext = nullptr;

    Pipeline *m_pPipeline = nullptr;

    Hash64 m_NameHash = ~0;
    RenderPassFlag m_Flags = RenderPassFlag::None;

    u32 m_ResourceCount = 0;
    RenderPassResource *m_pInputs = nullptr;
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(APIContext *, _Data &, RenderPassBuilder &)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(APIContext *, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(APIContext *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : RenderPass, _Data
{
    void Setup(APIContext *pContext, RenderPassBuilder *pBuilder) override
    {
        ZoneScoped;

        m_fSetup(pContext, (_Data &)(*this), (*pBuilder));
    }

    void Execute(APIContext *pContext, CommandList *pList) override
    {
        ZoneScoped;

        m_fExecute(pContext, (_Data &)(*this), pList);
    }

    void Shutdown(APIContext *pContext) override
    {
        ZoneScoped;

        m_fShutdown(pContext);
    }

    RenderPassSetupFn<_Data> m_fSetup = nullptr;
    RenderPassExecuteFn<_Data> m_fExecute = nullptr;
    RenderPassShutdownFn m_fShutdown = nullptr;
};

}  // namespace lr::Graphics