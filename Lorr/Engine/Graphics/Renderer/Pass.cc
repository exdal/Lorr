// Created on Monday March 6th 2023 by exdal
// Last modified on Monday June 26th 2023 by exdal
#include "Pass.hh"

namespace lr
{
void Graphics::InitPasses(RenderGraph *pGraph)
{
    ZoneScoped;

    AddSwapChainAcquirePass(pGraph, "$acquire");

    AddGeometryPass(pGraph, "geometry");
    AddImguiPass(pGraph, "imgui");

    AddSwapChainPresentPass(pGraph, "$present");
}

}  // namespace lr