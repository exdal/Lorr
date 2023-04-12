#include "Pass.hh"

namespace lr
{
void Graphics::InitPasses(RenderGraph *pGraph)
{
    ZoneScoped;

    AddSwapChainAcquirePass(pGraph, "$acquire");

    AddImguiPass(pGraph, "imgui");

    AddSwapChainPresentPass(pGraph, "$present");
}

}  // namespace lr