//
// Created on Thursday 10th November 2022 by e-erdal
//

#pragma once

#include "Resource.hh"

namespace lr::Resource
{
    constexpr u32 kResourceFileSignature = 1920102220;
    constexpr u16 kResourceMinVersion = 1;

    enum ResourceFileFlags : u32
    {
        LR_RESOURCE_FILE_FLAGS_NONE = 0,
        LR_RESOURCE_FILE_FLAGS_COMPRESSED = 1 << 0,
    };

    struct alignas(u32) ResourceFileHeader
    {
        u32 Signature = kResourceFileSignature;
        u32 Version = 1;
        u32 EngineVersion = 0;
        u32 GameVersion = 0xffffffff;
        u32 OriginalSize = 0xffffffff;
        ResourceFileFlags Flags = LR_RESOURCE_FILE_FLAGS_NONE;
        ResourceType Type = LR_RESOURCE_TYPE_UNKNOWN;
    };

}  // namespace lr::Resource