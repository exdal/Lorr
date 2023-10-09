// Created on Friday February 24th 2023 by exdal
#pragma once

#include "RenderPass.hh"
#include "DescriptorManager.hh"

#include "Crypt/xx.hh"
#include "Graphics/Device.hh"

namespace lr::Graphics
{
//
// struct PassData
// {
//     Image *m_pBackBuffer;
//     Image *m_pTestImage;
// };
//
// ...Prepare()...
// {
//     data.m_pBackBuffer = renderGraph.ImageIn("backbuffer")
//
//     ... do stuff ...
//
//     data.m_pTestImage = renderGraph.CreateImage("test_image", ...)
//
//     ... do stuff ...
//
//     renderGraph.ImageOut("test_image", MemoryAccess::ShaderRead)
// }
//

struct RenderGraph
{
    void Init();

    template<typename _Pass>
    _Pass *AllocateRenderPass()
    {
        void *pData = m_PassAllocator.Allocate(sizeof(_Pass));
        return new (pData) _Pass;
    }

    template<typename _PassData>
    RenderPass *CreateGraphicsPass(
        eastl::string_view name, RenderPassFlag flags = RenderPassFlag::None)
    {
        ZoneScoped;

        RenderPass *pPass = AllocateRenderPass<GraphicsRenderPassCallback<_PassData>>();
#ifdef LR_DEBUG
        pPass->m_Name = name;
#endif
        pPass->m_Hash = Hash::XXH3String(name);
        pPass->m_Flags = flags;

        if (m_pHeadPass == nullptr)
        {
            m_pHeadPass = pPass;
            pPass->m_pPrev = nullptr;
        }
        else
        {
            RenderPass *pLastPass = m_pHeadPass;
            while (pLastPass != nullptr && pLastPass->m_pNext != nullptr)
                pLastPass = pLastPass->m_pNext;

            pLastPass->m_pNext = pPass;
            pPass->m_pPrev = pLastPass;
        }

        return pPass;
    }

    template<typename _PassData>
    RenderPass *CreateGraphicsPass(
        eastl::string_view name,
        RenderPassSetupFn<_PassData> fSetup,
        RenderPassExecuteFn<_PassData> fExecute,
        RenderPassShutdownFn fShutdown,
        RenderPassFlag flags = RenderPassFlag::None)
    {
        ZoneScoped;

        auto *pPass =
            (GraphicsRenderPassCallback<_PassData> *)CreateGraphicsPass(name, flags);

        pPass->m_fSetup = fSetup;
        pPass->m_fExecute = fExecute;
        pPass->m_fShutdown = fShutdown;

        return pPass;
    }

    void Prepare();

    RenderPass *m_pHeadPass = nullptr;

    Buffer *m_pResourceBuffer = nullptr;  // memSize * n
    Buffer *m_pSamplerBuffer = nullptr;   // memSize * n
    eastl::fixed_vector<Buffer *, Limits::MaxFrameCount, false> m_BDABuffers = {};

    Memory::LinearAllocator m_PassAllocator = {};

    //DescriptorManager m_DescriptorMan = {};
    //APIContext *m_pContext = nullptr;
};

}  // namespace lr::Graphics