// Created on Thursday November 10th 2022 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#pragma once

#include "Resource.hh"

namespace lr::Resource
{
constexpr u32 kResourceFileSignature = 1920102220;  // 'Lorr'
constexpr u16 kResourceVersion = 1;
constexpr u16 kResourceMinVersion = 1;

enum class ResourceFileFlag : u32
{
    None = 0,
    Compressed = 1 << 0,
};

struct alignas(u32) ResourceFileHeader
{
    u32 m_Signature = kResourceFileSignature;
    u16 m_Version = kResourceVersion;
    u16 m_MinVersion = kResourceMinVersion;
    u32 m_EngineVersion = 0;
    u32 m_GameVersion = 0xffffffff;
    u32 m_OriginalSize = 0xffffffff;
    ResourceFileFlag m_Flags = ResourceFileFlag::None;
    ResourceType m_Type = ResourceType::Data;
};

}  // namespace lr::Resource
