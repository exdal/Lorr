//
// Created on Wednesday 6th July 2022 by exdal
//
// Reworked version of original implementation
//

#pragma once

#include "FileStream.hh"

namespace lr
{
    class BaseBufferStream
    {
    public:
        BaseBufferStream() = default;

        void Seek(u8 seekFlag, u32 position);
        void StartOver();

        void *Get(u32 dataLen, u32 count = 1);
        void *GetNew(u32 dataLen, u32 count = 1);
        eastl::string_view GetString(u32 dataSize);

        const u8 *GetData();
        const u8 *GetOffsetPtr();
        const u32 GetLength();
        const u32 GetOffset();

        template<typename T>
        T &Get()
        {
            ZoneScoped;

            T *pData = (T *)(m_pData + m_DataOffset);
            m_DataOffset += sizeof(T);
            return *pData;
        }

        template<typename T>
        eastl::string_view GetString()
        {
            ZoneScoped;

            u32 strLen = *(T *)Get(sizeof(T));

            return GetString(strLen);
        }

    protected:
        u8 *m_pData = nullptr;
        u32 m_DataLen = 0;
        u32 m_DataOffset = 0;
    };

    ////////////////////////////////

    class BufferReadStream : public BaseBufferStream
    {
    public:
        BufferReadStream(void *pData, u32 dataLen = -1);
        BufferReadStream(FileStream &fileStream);
    };

    ////////////////////////////////

    class BufferWriteStream : public BaseBufferStream
    {
    public:
        BufferWriteStream(u32 allocLen);
        BufferWriteStream(void *pData, u32 dataLen);
        ~BufferWriteStream();

        void Reset();
        void Reset(u32 dataLen);
        void Reset(u8 *pData, u32 dataLen);
        void Reset(BufferReadStream &stream);

        void Expand(u32 len);

        void Assign(void *pData, u32 dataLen, u32 count = 1);
        void AssignZero(u32 dataSize);
        void AssignString(const eastl::string &val);
        void AssignString(eastl::string_view val);

        template<typename T>
        void Assign(T &&val)
        {
            ZoneScoped;

            SmartAlloc(sizeof(T));

            memcpy(m_pData + m_DataOffset, &val, sizeof(T));
            m_DataOffset += sizeof(T);
        }

        template<typename T>
        void AssignString(const eastl::string &val)
        {
            ZoneScoped;

            size_t len = val.length();
            SmartAlloc(len);

            Assign(&len, sizeof(T));

            if (val.length())
                Assign((void *)val.data(), val.length());
        }

        template<typename T>
        void AssignString(eastl::string_view val)
        {
            ZoneScoped;

            size_t len = val.length();
            SmartAlloc(len);

            Assign(&len, sizeof(T));

            if (val.length())
                Assign((void *)val.data(), val.length());
        }

    private:
        void SmartAlloc(u32 insertedSize);
    };

}  // namespace lr