// Created on Thursday September 22nd 2022 by exdal
// Last modified on Sunday July 16th 2023 by exdal

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

    void Close();
    void SetOffset(u64 offset);

    template<typename _T>
    bool Read(_T &data);
    bool Read(eastl::string &str, u64 length);
    bool Read(u8 *pData, u64 dataSize);

    size_t Size() { return m_Size; }
    bool IsOK() { return m_pFile != nullptr; }

private:
    u64 m_Offset = ~0;
    u64 m_Size = ~0;
    FILE *m_pFile = nullptr;
};

}  // namespace lr