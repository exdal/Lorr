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

void RenderGraph::Init(APIContext *pContext)
{
    ZoneScoped;

    m_pContext = pContext;
    m_PassAllocator.Init({ .m_DataSize = 0xFFF0, .m_AllowMultipleBlocks = true });
    u32 frameCount = m_pContext->GetSwapChain()->m_FrameCount;

    DescriptorPoolDesc pPools[] = {
        { DescriptorType::UniformBuffer, Limits::kMaxBufferDescriptors },
        { DescriptorType::SampledImage, Limits::kMaxImageDescriptors },
        { DescriptorType::Sampler, Limits::kMaxSamplerDescriptors },
    };
    DescriptorManagerDesc descriptorManDesc = {
        .m_Pools = pPools,
        .m_pAPIContext = m_pContext,
    };
    m_DescriptorMan.Init(&descriptorManDesc);
}

void RenderGraph::Prepare()
{
    ZoneScoped;

    m_pResourceBuffer = nullptr;
    m_pSamplerBuffer = nullptr;
}

}  // namespace lr::Graphics