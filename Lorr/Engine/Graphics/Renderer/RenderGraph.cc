// Created on Friday February 24th 2023 by exdal
// Last modified on Saturday August 5th 2023 by exdal

#include "RenderGraph.hh"

namespace lr::Graphics
{
void RenderGraph::Init(APIContext *pContext)
{
    ZoneScoped;

    m_pResourceBuffer = nullptr;
    m_pSamplerBuffer = nullptr;
}

void RenderGraph::Prepare()
{
    ZoneScoped;
}

}  // namespace lr::Graphics