// Created on Thursday September 22nd 2022 by exdal
// Last modified on Saturday May 20th 2023 by exdal

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

    eastl::string_view GetString(u64 length = -1);
    u8 *GetPtr() { return m_Allocator.m_pData; }

    size_t Size() { return m_Allocator.m_View.m_Size; }
    bool IsOK() { return m_Allocator.m_View.m_Size != 0; }

private:
    Memory::LinearAllocator m_Allocator;
};

}  // namespace lr