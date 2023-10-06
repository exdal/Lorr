#pragma once

#include "Graphics/APIContext.hh"

namespace lr::Graphics
{
struct DescriptorPoolDesc
{
    DescriptorType m_Type = DescriptorType::Sampler;
    u64 m_MaxDescriptors = 0;
};

struct DescriptorPool
{
    DescriptorType m_Type = DescriptorType::Sampler;
    u64 m_MaxDescriptors = 0;
    u64 m_Offset = 0;
};

struct DescriptorManagerDesc
{
    eastl::span<DescriptorPoolDesc> m_Pools = {};
    APIContext *m_pAPIContext = nullptr;
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
    void SetDescriptors(eastl::span<DescriptorGetInfo> elements);

    Buffer *GetBuffer(DescriptorPoolType type);

    eastl::array<Buffer *, (u32)DescriptorPoolType::Count> m_Buffers;
    eastl::vector<DescriptorPool> m_Pools = {};
    PipelineLayout *m_pPipelineLayout = nullptr;
    APIContext *m_pContext = nullptr;
};

}  // namespace lr::Graphics