//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseSwapChain.hh"

#include "D3D12CommandQueue.hh"

namespace lr::Graphics
{
    struct D3D12SwapChainFrame
    {
    };

    struct D3D12SwapChain : public BaseSwapChain
    {
        IDXGISwapChain3 *m_pHandle = nullptr;
        DXGI_COLOR_SPACE_TYPE m_ColorSpace;
        DXGI_SWAP_EFFECT m_PresentMode;
    };

}  // namespace lr::Graphics