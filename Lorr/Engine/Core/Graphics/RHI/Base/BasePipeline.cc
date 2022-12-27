#include "BasePipeline.hh"

namespace lr::Graphics
{
    void BasePipelineBuildInfo::SetDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets, BaseDescriptorSet *pSamplerSet)
    {
        ZoneScoped;

        m_pSamplerDescriptorSet = pSamplerSet;

        m_DescriptorSetCount = sets.size();
        memcpy(m_ppDescriptorSets, sets.begin(), sets.size() * PTR_SIZE);
    }

    PushConstantDesc &BasePipelineBuildInfo::AddPushConstant()
    {
        ZoneScoped;

        return m_pPushConstants[m_PushConstantCount++];
    }

}  // namespace lr::Graphics