//
// Created on Wednesday 21st September 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Base/BasePipeline.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKPipeline : Pipeline
    {
        VkPipeline m_pHandle = nullptr;
        VkPipelineLayout m_pLayout = nullptr;
    };

}  // namespace lr::Graphics