// Created on Friday February 24th 2023 by exdal
#include "RenderGraph.hh"

namespace lr::Graphics
{
namespace Limits
{
    constexpr u32 kMaxBufferDescriptors = 256;
    constexpr u32 kMaxImageDescriptors = 256;
    constexpr u32 kMaxSamplerDescriptors = 64;
}  // namespace Limits

void RenderGraph::Init()
{
    ZoneScoped;

    m_PassAllocator.Init({ .m_DataSize = 0xFFF0, .m_AllowMultipleBlocks = true });
}

void RenderGraph::Prepare()
{
    ZoneScoped;

    m_pResourceBuffer = nullptr;
    m_pSamplerBuffer = nullptr;
}

}  // namespace lr::Graphics