#include "FileSystem.hh"

namespace lr::fs {
FileView::FileView(std::string_view path)
{
    ZoneScoped;

    m_pFile = fopen(path.data(), "rb");
    if (!is_ok())
        return;

    fseek(m_pFile, 0, SEEK_END);
    m_Size = ftell(m_pFile);
    rewind(m_pFile);
}

FileView::~FileView()
{
    ZoneScoped;

    close();
}

void FileView::close()
{
    ZoneScoped;

    if (is_ok())
        fclose(m_pFile);
}

void FileView::set_offset(u32 offset)
{
    ZoneScoped;

    fseek(m_pFile, offset, SEEK_SET);
    m_Offset = offset;
}

template<typename T>
bool FileView::read(T &data)
{
    ZoneScoped;

    return read((u8 *)&data, sizeof(T));
}

bool FileView::read(std::string &str, u64 length)
{
    ZoneScoped;

    str.resize(length);
    return read((u8 *)str.data(), length);
}

bool FileView::read(u8 *pData, u64 dataSize)
{
    ZoneScoped;

    if (m_Offset + dataSize >= m_Size)
        return false;

    fread(pData, dataSize, 1, m_pFile);

    return true;
}

std::string read_file(std::string_view path)
{
    ZoneScoped;

    FileView file(path);
    if (!file.is_ok())
        return {};

    std::string result;
    file.read(result, file.size());
    return result;
}
}  // namespace lr::fs
