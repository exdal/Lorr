#include "FileStream.hh"

#include "BufferStream.hh"

namespace lr
{
    FileStream::FileStream(eastl::string_view path, bool write)
    {
        ZoneScoped;

        if (write)
        {
            m_File = fopen(path.data(), "wb");
        }
        else
        {
            m_File = fopen(path.data(), "rb");
        }

        if (!IsOK())
            return;

        GetSize();
    }

    void FileStream::Reopen(eastl::string_view path, bool write)
    {
        ZoneScoped;

        if (IsOK())
            Close();

        if (write)
        {
            m_File = fopen(path.data(), "wb");
        }
        else
        {
            m_File = fopen(path.data(), "rb");
        }

        if (!IsOK())
            return;

        GetSize();
    }

    void FileStream::Close()
    {
        ZoneScoped;

        fclose(m_File);
    }

    void FileStream::WriteString(eastl::string_view val)
    {
        ZoneScoped;

        fwrite(val.data(), 1, val.length(), m_File);
    }

    void FileStream::GetSize()
    {
        ZoneScoped;

        if (m_FileSize > 0)
            return;

        fseek(m_File, 0, SEEK_END);
        m_FileSize = ftell(m_File);
        rewind(m_File);
    }

}  // namespace lr