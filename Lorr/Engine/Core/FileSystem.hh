#pragma once

namespace lr::fs {
class FileView {
public:
    FileView(std::string_view path);
    ~FileView();

    void close();
    void set_offset(u32 offset);

    template<typename T>
    bool read(T &data);
    bool read(std::string &str, u64 length);
    bool read(u8 *pData, u64 dataSize);

    usize size() { return m_Size; }
    bool is_ok() { return m_pFile != nullptr; }

private:
    u64 m_Offset = ~0;
    u64 m_Size = ~0;
    FILE *m_pFile = nullptr;
};

}  // namespace lr::fs
