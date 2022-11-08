//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr::Graphics
{
    enum class APIFlags : u32
    {
        None,

        VSync = 1 << 0,
    };
    EnumFlags(APIFlags);

    enum class TextureFiltering : u8
    {
        Point = 0,
        Linear,
        Ansio,

        Count
    };

    enum class TextureAddress : u8
    {
        Wrap,
        Mirror,
        Clamp,
        Border,
    };

}  // namespace lr::Graphics