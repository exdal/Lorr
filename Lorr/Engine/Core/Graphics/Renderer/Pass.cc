#include "Pass.hh"

namespace lr
{
    void Graphics::InitPasses(RenderGraph *pGraph)
    {
        ZoneScoped;

        // Passes start with '$' are considered special, kind of hardcoded.
        // They are always expected to exist and must be created before execution,
        // and must not be deleted.
        // Current special names:
        //~ List of names MUST NOT be a member of any group:
        //* $acquire = Pass name, start of rendering.
        //* $present = Pass name, end of rendering.
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //* $head = Head of graph, groups who bound this name will be executed right after `$acquire`.

        AddSwapChainAcquirePass(pGraph, "$acquire");
        AddSwapChainPresentPass(pGraph, "$present");

        AddImguiPass(pGraph, "imgui");
    }

}  // namespace lr