//
// Created on Sunday 23rd October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

namespace lr::Graphics
{
    struct ClearValue
    {
        ClearValue()
        {
        }

        ClearValue(const XMFLOAT4 &color)
        {
            IsDepth = false;

            RenderTargetColor = color;
        }

        ClearValue(f32 depth, u8 stencil)
        {
            IsDepth = true;

            DepthStencilColor.Depth = depth;
            DepthStencilColor.Stencil = stencil;
        }

        XMFLOAT4 RenderTargetColor = {};

        struct Depth
        {
            f32 Depth = 0.f;
            u8 Stencil = 0;
        } DepthStencilColor;

        bool IsDepth = false;
    };

    struct CommandRenderPassBeginInfo
    {
        u32 ClearValueCount = 0;
        ClearValue pClearValues[APIConfig::kMaxColorAttachmentCount] = {};

        // WH(-1) means we cover entire window/screen, info from swapchain
        XMUINT4 RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };
    };

    struct BaseCommandAllocator
    {
    };

    struct BaseCommandList
    {
        CommandListType m_Type;
        BaseCommandAllocator *m_pAllocator = nullptr;
    };

    struct CommandListPool
    {
        void Init();

        BaseCommandList *Acquire(CommandListType type);
        void Release(BaseCommandList *pList);
        void SignalFence(BaseCommandList *pList);
        void ReleaseFence(u32 index);

        void WaitForAll();

        eastl::array<BaseCommandList *, 32> m_DirectLists;
        eastl::array<BaseCommandList *, 16> m_ComputeLists;
        eastl::array<BaseCommandList *, 8> m_CopyLists;

        eastl::atomic<u32> m_DirectListMask;
        eastl::atomic<u32> m_DirectFenceMask;

        eastl::atomic<u16> m_ComputeListMask;
        eastl::atomic<u8> m_CopyListMask;
    };

}  // namespace lr::Graphics