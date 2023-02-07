//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

#include "BasePipeline.hh"
#include "BaseResource.hh"

namespace lr::Graphics
{
    enum class SwapChainFlags : u8
    {
        None,
        vSync = 1 << 0,
        AllowTearing = 1 << 1,  // TODO
        TripleBuffering = 1 << 2,
    };
    EnumFlags(SwapChainFlags);

    struct BaseSwapChain
    {
        virtual Image *GetCurrentImage() = 0;
        void NextFrame();

        u32 m_CurrentFrame = 0;
        u32 m_FrameCount = 2;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_vSync = false;

        ImageFormat m_ImageFormat = LR_IMAGE_FORMAT_RGBA8F;
    };

};  // namespace lr::Graphics