//
// Created on Sunday 9th October 2022 by e-erdal
//

#pragma once

#include "Core/IO/Memory.hh"

namespace lr::Graphics::APIConfig
{
    constexpr u32 kMaxCommandListPerBatch = 8;

    constexpr u32 kMaxSubpassCount = 8;
    constexpr u32 kMaxColorAttachmentCount = 8;

    constexpr u32 kTransientBufferSize = Memory::MiBToBytes(64);
    constexpr u32 kTransientBufferCount = 1 << 15;

}  // namespace lr::Graphics::APIConfig