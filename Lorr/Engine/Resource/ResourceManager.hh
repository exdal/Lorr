// Created on Monday May 15th 2023 by exdal
// Last modified on Monday May 15th 2023 by exdal

#pragma once

#include "Memory/Allocator/TLSFAllocator.hh"

namespace lr::Resource
{
struct ResourceManager
{
    void Init(eastl::string_view metaPath);

    Memory::TLSFAllocator m_Allocator;
};
}  // namespace lr::Resource
