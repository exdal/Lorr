//
// Created on Saturday 16th July 2022 by e-erdal
//

#pragma once

#include "Crypto/Light.hh"

namespace lr::Graphics
{
    template<typename PipelineStateHandleT>
    struct APIPipelineStateCache
    {
        void Init()
        {
            memset(&m_Hashes[0], 0, sizeof(m_Hashes[0]) * kMaxPipelineStateCount);
            memset(&m_Handles[0], 0, sizeof(void *) * kMaxPipelineStateCount);
        }

        PipelineStateHandleT *Get(u32 hash)
        {
            ZoneScoped;

            u32 idx = 0;
            for (auto &currentHash : m_Hashes)
            {
                idx++;
                if (currentHash == hash)
                {
                    return m_Handles[idx];
                }
            }

            return nullptr;
        }

        void Add(PipelineStateHandleT *pHandle, u32 hash)
        {
            ZoneScoped;

            m_Hashes[m_HandleSize] = hash;
            m_Handles[m_HandleSize] = pHandle;

            m_HandleSize++;
        }

        static constexpr u32 kMaxPipelineStateCount = 128;

        eastl::array<u32, kMaxPipelineStateCount> m_Hashes;
        eastl::array<PipelineStateHandleT *, kMaxPipelineStateCount> m_Handles;
        u32 m_HandleSize = 0;
    };

}  // namespace lr::Graphics