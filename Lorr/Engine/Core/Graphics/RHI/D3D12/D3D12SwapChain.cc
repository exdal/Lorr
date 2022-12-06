#include "D3D12SwapChain.hh"

#include "D3D12API.hh"

namespace lr::Graphics
{
    void D3D12SwapChain::Init(BaseWindow *pWindow, D3D12API *pAPI, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & SwapChainFlags::vSync);
        (flags & SwapChainFlags::TripleBuffering) ? m_FrameCount = 3 : 2;

        // Calculate window metrics
        m_Width = pWindow->m_Width;
        m_Height = pWindow->m_Height;

        CreateHandle(pAPI, pWindow);
    }

    void D3D12SwapChain::Present()
    {
        ZoneScoped;

        m_pHandle->Present(0, 0);
    }

    void D3D12SwapChain::CreateHandle(D3D12API *pAPI, BaseWindow *pWindow)
    {
        ZoneScoped;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainDescFS = {};

        swapChainDesc.Width = m_Width;
        swapChainDesc.Height = m_Height;
        swapChainDesc.Format = D3D12API::ToDXFormat(m_ImageFormat);

        swapChainDesc.Stereo = false;

        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;

        swapChainDesc.BufferCount = m_FrameCount;
        swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;

        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        IDXGISwapChain1 *pHandle1 = nullptr;
        pAPI->CreateSwapChain(pHandle1, pWindow->m_pHandle, swapChainDesc, nullptr);

        pHandle1->QueryInterface(IID_PPV_ARGS(&m_pHandle));
        SAFE_RELEASE(pHandle1);

        m_CurrentFrame = m_pHandle->GetCurrentBackBufferIndex();
    }

    void D3D12SwapChain::CreateBackBuffers(D3D12API *pAPI)
    {
        ZoneScoped;

        ImageDesc imageDesc;
        imageDesc.Format = m_ImageFormat;

        ImageData imageData;
        imageData.Width = m_Width;
        imageData.Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            D3D12SwapChainFrame &frame = m_pFrames[i];
            D3D12Image &currentImage = frame.Image;

            // Swapchain already gives us the image, so we don't need to create it again
            m_pHandle->GetBuffer(i, IID_PPV_ARGS(&currentImage.m_pHandle));

            currentImage.m_Width = m_Width;
            currentImage.m_Height = m_Height;
            currentImage.m_DataSize = ~0;
            currentImage.m_UsingMip = 0;
            currentImage.m_TotalMips = 1;
            currentImage.m_Usage = ResourceUsage::Present;
            currentImage.m_Format = m_ImageFormat;
            currentImage.m_RequiredDataSize = ~0;
            currentImage.m_AllocatorType = AllocatorType::Count;

            pAPI->CreateImageView(&currentImage);
        }
    }

    BaseImage *D3D12SwapChain::GetCurrentImage()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame].Image;
    }

    D3D12SwapChainFrame *D3D12SwapChain::GetCurrentFrame()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame];
    }

    D3D12SwapChainFrame *D3D12SwapChain::GetNextFrame()
    {
        ZoneScoped;

        u32 nextFrameIdx = (m_CurrentFrame + 1) % m_FrameCount;
        return &m_pFrames[nextFrameIdx];
    }

    void D3D12SwapChain::DestroyHandle(D3D12API *pAPI)
    {
        ZoneScoped;

        // for (u32 i = 0; i < m_FrameCount; i++)
        // {
        //     D3D12SwapChainFrame &frame = m_pFrames[i];
        // }

        // pAPI->DeleteSwapChain(m_pHandle);
    }

}  // namespace lr::Graphics