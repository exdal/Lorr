// Created on Thursday September 22nd 2022 by exdal
// Last modified on Friday June 30th 2023 by exdal
#include "File.hh"

// #include "BufferStream.hh"

namespace lr
{
FileView::FileView(eastl::string_view path)
{
    ZoneScoped;

    FILE *pFile = fopen(path.data(), "rb");
    if (!pFile)
        return;

    fseek(pFile, 0, SEEK_END);
    m_Size = ftell(pFile);
    rewind(pFile);

    m_Allocator.Init({ m_Size });
    fread(m_Allocator.Allocate(m_Size), m_Size, 1, pFile);
    fclose(pFile);
}

FileView::~FileView()
{
    ZoneScoped;

    m_Allocator.Delete();
}

eastl::string_view FileView::GetAsString(u64 length)
{
    ZoneScoped;

    length = eastl::min(length, m_Size);
    return eastl::string_view((const char *)m_Allocator.GetFirstRegion()->m_pData, length);
}

u8 *FileView::GetPtr()
{
    return m_Allocator.GetFirstRegion()->m_pData;
}

}  // namespace lr