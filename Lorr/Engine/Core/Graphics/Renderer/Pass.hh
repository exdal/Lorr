//
// Created on Wednesday 23rd November 2022 by exdal
//

#pragma once

#include "Core/Graphics/RenderGraph/RenderGraph.hh"

namespace lr::Graphics
{
    void AddSwapChainAcquirePass(RenderGraph *pGraph, eastl::string_view name);
    void AddSwapChainPresentPass(RenderGraph *pGraph, eastl::string_view name);
    void AddImguiPass(RenderGraph *pGraph, eastl::string_view name);

    void InitPasses(RenderGraph *pGraph);
}