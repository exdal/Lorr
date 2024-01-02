#pragma once

namespace lr::fs
{
class FileView
{
public:
    FileView(eastl::string_view path);
    ~FileView();

    void close();
    void set_offset(u64 offset);

    template<typename _T>
    bool read(_T &data);
    bool read(eastl::string &str, u64 length);
    bool read(u8 *pData, u64 dataSize);

    usize size() { return m_Size; }
    bool is_ok() { return m_pFile != nullptr; }

private:
    u64 m_Offset = ~0;
    u64 m_Size = ~0;
    FILE *m_pFile = nullptr;
};

eastl::string read_file(eastl::string_view path);
eastl::string get_current_dir();

bool set_library_dir(eastl::string_view path);
void *load_lib(eastl::string_view path);
}  // namespace lr::fs