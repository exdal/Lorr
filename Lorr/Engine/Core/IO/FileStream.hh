//
// Created on Wednesday 6th July 2022 by exdal
//

#pragma once

namespace lr
{
    class FileStream
    {
    public:
        FileStream() = default;
        FileStream(eastl::string_view path, bool write);
        void Reopen(eastl::string_view path, bool write);

        void Close();

        /// STL FUNCTIONS DON'T OWN MEMORY, YOU NEED TO FREE IT YOURSELF
        /// actually implemented a special function for reading string
        template<typename T>
        inline T *ReadAll()
        {
            GetSize();

            T *pBuffer = (T *)malloc(m_FileSize);
            fread(pBuffer, 1, m_FileSize, m_File);
            return pBuffer;
        }

        template<typename T>
        inline T *ReadPtr(size_t size = 0)
        {
            T *pBuffer = nullptr;

            if (size == 0)
            {
                pBuffer = (T *)malloc(sizeof(T));
                fread(pBuffer, sizeof(T), 1, m_File);
            }
            else
            {
                pBuffer = (T *)malloc(size);
                fread(pBuffer, size, 1, m_File);
            }

            return pBuffer;
        }

        template<typename T>
        inline T &Read(size_t size = 0)
        {
            T *buffer;
            fread(buffer, (size == 0 ? sizeof(T) : size), 1, m_File);
            return *buffer;
        }

        inline eastl::string ReadString(size_t size)
        {
            return eastl::string(ReadPtr<char>(size), size);
        }

        template<typename T>
        inline void Write(const T &t, size_t size = 0)
        {
            if (size > 0)
                fwrite(&t, 1, size, m_File);
            else
                fwrite(&t, 1, sizeof(T), m_File);
        }

        void WriteString(eastl::string_view val);

    private:
        void GetSize();

    public:
        size_t Size()
        {
            return m_FileSize;
        }

        bool IsOK()
        {
            return m_File;
        }

    private:
        FILE *m_File = 0;
        u32 m_FileSize = 0;
    };

}  // namespace lr