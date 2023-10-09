#pragma once

#include "Graphics/APIObject.hh"

namespace lr::Renderer
{

struct DescriptorPoolDesc
{
    Graphics::DescriptorType m_Type = Graphics::DescriptorType::Sampler;
    u64 m_MaxDescriptors = 0;
};

struct DescriptorPool
{
    Graphics::DescriptorType m_Type = Graphics::DescriptorType::Sampler;
    u64 m_MaxDescriptors = 0;
    u64 m_Offset = 0;
};

struct DescriptorManagerDesc
{
    eastl::span<DescriptorPoolDesc> m_Pools = {};
    Graphics::Device *m_pDevice = nullptr;
};

enum class DescriptorPoolType
{
    BDA,
    Resource,
    Sampler,

    Count,
};

struct DescriptorManager
{
    void Init(DescriptorManagerDesc *pDesc);
    void InitBDAPool(usize count = 0);
    void SetDescriptors(eastl::span<Graphics::DescriptorGetInfo> elements);

    Graphics::Buffer *GetBuffer(DescriptorPoolType type);

    eastl::array<Graphics::Buffer *, (u32)DescriptorPoolType::Count> m_Buffers;
    eastl::vector<DescriptorPool> m_Pools = {};
    Graphics::PipelineLayout *m_pPipelineLayout = nullptr;
    Graphics::Device *m_pDevice = nullptr;
};

}  // namespace lr::Renderer