// Created on Friday February 24th 2023 by exdal
// Last modified on Friday August 18th 2023 by exdal

#pragma once

#include "RenderPass.hh"

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

        auto *pPass = AllocateRenderPassCallback<GraphicsRenderPassCallback<_PassData>>();
        pPass->m_NameHash = Hash::FNV64String(name);
        pPass->m_Flags = flags;
        pPass->m_PassType = CommandType::Graphics;
    }

    void Prepare();

    RenderPass *m_pHeadRenderPass = nullptr;

    Buffer *m_pResourceBuffer = nullptr;  // memSize * n
    Buffer *m_pSamplerBuffer = nullptr;   // memSize * n
    eastl::fixed_vector<Buffer *, LR_MAX_FRAME_COUNT, false> m_BDABuffers = {};

    Memory::LinearAllocator m_PassAllocator = {};

    APIContext *m_pContext = nullptr;
};

}  // namespace lr::Graphics