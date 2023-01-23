//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseSwapChain.hh"

#include "D3D12CommandQueue.hh"
#include "D3D12Resource.hh"

namespace lr::Graphics
{
    struct D3D12SwapChainFrame
    {
        D3D12Image m_Image;
    };

    struct D3D12API;
    struct D3D12SwapChain : public BaseSwapChain
    {
        void Init(BaseWindow *pWindow, D3D12API *pAPI, SwapChainFlags flags);

        void Present();

        void CreateHandle(D3D12API *pAPI, BaseWindow *pWindow);
        void CreateBackBuffers(D3D12API *pAPI);
        Image *GetCurrentImage();
        D3D12SwapChainFrame *GetCurrentFrame();
        D3D12SwapChainFrame *GetNextFrame();

        void ResizeBuffers();
        void DeleteBuffers(D3D12API *pAPI);

        /// INTERNALS ///
        IDXGISwapChain3 *m_pHandle = nullptr;
        DXGI_COLOR_SPACE_TYPE m_ColorSpace;
        DXGI_SWAP_EFFECT m_PresentMode;

        D3D12SwapChainFrame m_pFrames[3] = {};
    };

}  // namespace lr::Graphics