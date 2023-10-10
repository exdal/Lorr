#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/PhysicalDevice.hh"

namespace lr
{
struct BaseWindow;
}

namespace lr::Renderer
{
struct Renderer
{
    void Init(BaseWindow *pWindow);

    Graphics::Instance m_Instance = {};
    Graphics::PhysicalDevice *m_pPhysicalDevice = nullptr;
    Graphics::Surface *m_pSurface = nullptr;
    Graphics::Device *m_pDevice = nullptr;
    Graphics::SwapChain *m_pSwapChain = nullptr;
};
}  // namespace lr::Renderer