// Created on Thursday September 22nd 2022 by exdal
// Last modified on Saturday May 20th 2023 by exdal
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
    u32 fileSize = ftell(pFile);
    rewind(pFile);

    m_Allocator.Init({ .m_DataSize = fileSize });
    fread(m_Allocator.m_pData, fileSize, 1, pFile);

    fclose(pFile);
}

FileView::~FileView()
{
    ZoneScoped;

    m_Allocator.Free(true);
}

eastl::string_view FileView::GetString(u64 length)
{
    ZoneScoped;

    length = eastl::min(length, m_Allocator.m_View.m_Size);

    const char *pString = (const char *)m_Allocator.Allocate(length);
    return eastl::string_view(pString, length);
}

}  // namespace lr