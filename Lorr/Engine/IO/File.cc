#include "File.hh"

namespace lr
{
FileView::FileView(eastl::string_view path)
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

void FileView::set_offset(u64 offset)
{
    ZoneScoped;

    fseek(m_pFile, offset, SEEK_SET);
    m_Offset = offset;
}

template<typename _T>
bool FileView::read(_T &data)
{
    ZoneScoped;

    return read((u8 *)&data, sizeof(_T));
}

bool FileView::read(eastl::string &str, u64 length)
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

eastl::string FileUtils::read_file(eastl::string_view path)
{
    ZoneScoped;

    FileView file(path);
    eastl::string result;
    file.read(result, file.size());
    return result;
}

}  // namespace lr