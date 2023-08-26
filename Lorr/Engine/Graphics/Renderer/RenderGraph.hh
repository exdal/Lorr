// Created on Friday February 24th 2023 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

#include "RenderPass.hh"
#include "Crypt/CRC.hh"

#include "Graphics/APIContext.hh"

namespace lr::Graphics
{
struct RenderGraph
{
    void Init(APIContext *pContext);

    template<typename _Pass>
    _Pass *AllocateRenderPass()
    {
        void *pData = m_PassAllocator.Allocate(sizeof(_Pass));
        return new (pData) _Pass;
    }

    template<typename _PassData>
    RenderPass *CreateGraphicsPass(eastl::string_view name, RenderPassFlag flags = RenderPassFlag::None)
    {
        ZoneScoped;

        RenderPass *pPass = AllocateRenderPassCallback<GraphicsRenderPassCallback<_PassData>>();
        pPass->m_Name = name;
        pPass->m_Hash = Hash::CRC32String(Hash::CRC32Data_SW, name);
        pPass->m_Flags = flags;

        return pPass;
    }

    void Prepare();

    RenderPass *m_pHeadRenderPass = nullptr;

    Buffer *m_pResourceBuffer = nullptr;  // memSize * n
    Buffer *m_pSamplerBuffer = nullptr;   // memSize * n
    eastl::fixed_vector<Buffer *, Limits::MaxFrameCount, false> m_BDABuffers = {};

    Memory::LinearAllocator m_PassAllocator = {};

    APIContext *m_pContext = nullptr;
};

}  // namespace lr::Graphics