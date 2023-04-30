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