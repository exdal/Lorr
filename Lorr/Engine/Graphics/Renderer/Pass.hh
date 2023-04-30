//
// Created on Wednesday 23rd November 2022 by exdal
//

#pragma once

#include "Graphics/RenderGraph/RenderGraph.hh"

#define DEFINE_PASS(name) void Add##name##Pass(RenderGraph *pGraph, eastl::string_view name)

namespace lr::Graphics
{
    DEFINE_PASS(SwapChainAcquire);
    DEFINE_PASS(SwapChainPresent);
    DEFINE_PASS(Imgui);
    DEFINE_PASS(Geometry);

    void InitPasses(RenderGraph *pGraph);
}

#undef DEFINE_PASS