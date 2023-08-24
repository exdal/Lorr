// Created on Friday February 24th 2023 by exdal
// Last modified on Wednesday August 23rd 2023 by exdal

#pragma once

#include <EASTL/fixed_string.h>

#include "Crypt/FNV.hh"
#include "Graphics/APIContext.hh"

namespace lr::Graphics
{
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

    RenderPassFlag m_Flags = RenderPassFlag::None;
    u32 m_Hash = 0;

#if _DEBUG
    eastl::string m_Name;
#endif
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(APIContext *, _Data &, RenderPassBuilder &)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(APIContext *, _Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(APIContext *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : RenderPass, _Data
{
    using PipelineBuildInfo_t = GraphicsPipelineBuildInfo;
    using PassData_t = _Data;

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