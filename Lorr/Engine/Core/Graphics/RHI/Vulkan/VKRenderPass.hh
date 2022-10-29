//
// Created on Wednesday 26th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseRenderPass.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKRenderPass : BaseRenderPass
    {
        VkRenderPass m_pHandle = nullptr;
    };

}  // namespace lr::Graphics