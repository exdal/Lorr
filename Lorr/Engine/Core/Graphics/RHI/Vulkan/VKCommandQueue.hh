//
// Created on Sunday 22nd May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"

#include "VKSym.hh"
#include "VKCommandList.hh"

namespace lr::Graphics
{
    struct VKCommandQueue
    {
        void Init(VkQueue pHandle);

        VkQueue m_pHandle = nullptr;

        bool m_IsHighPriortiy = false;
        CommandListType m_Type;
    };

}  // namespace lr::Graphics