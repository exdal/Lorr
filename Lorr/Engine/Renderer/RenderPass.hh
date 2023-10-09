// Created on Friday February 24th 2023 by exdal
// Last modified on Tuesday August 29th 2023 by exdal

#pragma once

#include <EASTL/fixed_string.h>

#include "RenderPassResource.hh"

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

enum class RenderPassFlag : u32
{
    None = 0,
    AllowExecute = 1 << 0,
    AllowShutdown = 1 << 1,
    AllowSetup = 1 << 2,
    SkipRendering = 1 << 3,
};

struct RenderGraph;
struct RenderPass
{
    virtual void Setup(RenderGraph *pGraph) = 0;
    virtual void Execute(CommandList *pList) = 0;
    virtual void Shutdown(RenderGraph *pGraph) = 0;

    RenderPass *m_pPrev = nullptr;
    RenderPass *m_pNext = nullptr;

    Pipeline *m_pPipeline = nullptr;
    RenderPassFlag m_Flags = RenderPassFlag::None;

    u64 m_Hash = 0;
#if LR_DEBUG
    eastl::string m_Name;
#endif

    // Keep it on bottom
    eastl::span<RenderPassResource> m_Resources;
};

template<typename _Data>
using RenderPassSetupFn = eastl::function<void(_Data &, RenderGraph &)>;
template<typename _Data>
using RenderPassExecuteFn = eastl::function<void(_Data &, CommandList *)>;
using RenderPassShutdownFn = eastl::function<void(RenderGraph *)>;

template<typename _Data>
struct GraphicsRenderPassCallback : RenderPass, _Data
{
    using PipelineBuildInfo_t = GraphicsPipelineBuildInfo;
    using PassData_t = _Data;

    void Setup(RenderGraph *pGraph) override
    {
        ZoneScoped;

        m_fSetup((_Data &)(*this), (*pGraph));
    }

    void Execute(CommandList *pList) override
    {
        ZoneScoped;

        m_fExecute((_Data &)(*this), pList);
    }

    void Shutdown(RenderGraph *pGraph) override
    {
        ZoneScoped;

        m_fShutdown(pGraph);
    }

    RenderPassSetupFn<_Data> m_fSetup = nullptr;
    RenderPassExecuteFn<_Data> m_fExecute = nullptr;
    RenderPassShutdownFn m_fShutdown = nullptr;
};

}  // namespace lr::Graphics