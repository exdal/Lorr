#include "D3D12SwapChain.hh"

#include "D3D12API.hh"

#undef LOG_SET_NAME
#define LOG_SET_NAME "D3D12SC"

namespace lr::Graphics
{
    void D3D12SwapChain::Init(BaseWindow *pWindow, D3D12API *pAPI, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & LR_SWAP_CHAIN_FLAG_VSYNC);
        (flags & LR_SWAP_CHAIN_FLAG_TRIPLE_BUFFERING) ? m_FrameCount = 3 : 2;

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
        imageDesc.m_Format = m_ImageFormat;
        imageDesc.m_Width = m_Width;
        imageDesc.m_Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            D3D12SwapChainFrame &frame = m_pFrames[i];
            D3D12Image &currentImage = frame.m_Image;

            // Swapchain already gives us the image, so we don't need to create it again
            m_pHandle->GetBuffer(i, IID_PPV_ARGS(&currentImage.m_pHandle));

            currentImage.m_Width = m_Width;
            currentImage.m_Height = m_Height;
            currentImage.m_DataLen = ~0;
            currentImage.m_MipMapLevels = 1;
            currentImage.m_UsageFlags = LR_RESOURCE_USAGE_PRESENT;
            currentImage.m_Format = m_ImageFormat;
            currentImage.m_DeviceDataLen = ~0;
            currentImage.m_TargetAllocator = LR_API_ALLOCATOR_COUNT;

            pAPI->CreateImageView(&currentImage);
        }

        m_CurrentFrame = 0;
    }

    Image *D3D12SwapChain::GetCurrentImage()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame].m_Image;
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

    void D3D12SwapChain::ResizeBuffers()
    {
        ZoneScoped;

        m_pHandle->ResizeBuffers(m_FrameCount, m_Width, m_Height, D3D12API::ToDXFormat(m_ImageFormat), 0);
    }

    void D3D12SwapChain::DeleteBuffers(D3D12API *pAPI)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            D3D12SwapChainFrame &frame = m_pFrames[i];
            pAPI->DeleteImage(&frame.m_Image);
        }
    }

}  // namespace lr::Graphics