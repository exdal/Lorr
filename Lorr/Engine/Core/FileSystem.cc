#include "FileSystem.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <Windows.h>

#include <EASTL/finally.h>

namespace lr::fs
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

eastl::string read_file(eastl::string_view path)
{
    ZoneScoped;

    FileView file(path);
    if (!file.is_ok())
        return {};

    eastl::string result;
    file.read(result, file.size());
    return result;
}

eastl::string get_current_dir()
{
    ZoneScoped;

    char *buffer = _getcwd(nullptr, 0);
    auto _ = eastl::make_finally([&] { free(buffer); });
    return buffer;
}

bool set_library_dir(eastl::string_view path)
{
    ZoneScoped;

    return SetDllDirectoryA(path.data());
}

void *load_lib(eastl::string_view path)
{
    ZoneScoped;

    return LoadLibraryA(path.data());
}

void free_lib(void *lib)
{
    ZoneScoped;

    FreeLibrary(static_cast<HMODULE>(lib));
}

void *get_lib_func(void *lib, eastl::string_view func_name)
{
    ZoneScoped;

    return GetProcAddress(static_cast<HMODULE>(lib), func_name.data());
}
}  // namespace lr::fs