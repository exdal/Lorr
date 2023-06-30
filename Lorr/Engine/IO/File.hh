// Created on Thursday September 22nd 2022 by exdal
// Last modified on Friday June 30th 2023 by exdal

#pragma once

#include "Memory/Allocator/LinearAllocator.hh"

namespace lr
{
enum FileOperation
{
    Write,
    Read,
    Append,
};

class FileView
{
public:
    FileView(eastl::string_view path);
    ~FileView();

    eastl::string_view GetAsString(u64 length = -1);
    u8 *GetPtr();

    size_t Size() { return m_Size; }
    bool IsOK() { return m_Size != ~0; }

private:
    u64 m_Size = ~0;
    Memory::LinearAllocator m_Allocator = {}; 
};

}  // namespace lr