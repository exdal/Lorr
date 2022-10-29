#include "BasePipeline.hh"

namespace lr::Graphics
{
    void GraphicsPipelineBuildInfo::SetDescriptorSets(std::initializer_list<BaseDescriptorSet *> sets)
    {
        ZoneScoped;

        m_DescriptorSetCount = sets.size();
        memcpy(m_ppDescriptorSets, sets.begin(), sets.size() * PTR_SIZE);
    }

}  // namespace lr::Graphics