#pragma once

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

namespace FileUtils
{
    eastl::string read_file(eastl::string_view path);
}

}  // namespace lr