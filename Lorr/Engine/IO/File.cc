// Created on Thursday September 22nd 2022 by exdal
// Last modified on Sunday July 16th 2023 by exdal
#include "File.hh"

// #include "BufferStream.hh"

namespace lr
{
FileView::FileView(eastl::string_view path)
{
    ZoneScoped;

    m_pFile = fopen(path.data(), "rb");
    if (!IsOK())
        return;

    fseek(m_pFile, 0, SEEK_END);
    m_Size = ftell(m_pFile);
    rewind(m_pFile);
}

FileView::~FileView()
{
    ZoneScoped;

    Close();
}

void FileView::Close()
{
    ZoneScoped;

    if (IsOK())
        fclose(m_pFile);
}

void FileView::SetOffset(u64 offset)
{
    ZoneScoped;

    fseek(m_pFile, offset, SEEK_SET);
    m_Offset = offset;
}

template<typename _T>
bool FileView::Read(_T &data)
{
    ZoneScoped;

    return Read((u8 *)&data, sizeof(_T));
}

bool FileView::Read(eastl::string &str, u64 length)
{
    ZoneScoped;

    str.reserve(length);
    return Read((u8 *)str.data(), length);
}

bool FileView::Read(u8 *pData, u64 dataSize)
{
    ZoneScoped;

    if (m_Offset + dataSize >= m_Size)
        return false;

    fread(pData, dataSize, 1, m_pFile);

    return true;
}

}  // namespace lr