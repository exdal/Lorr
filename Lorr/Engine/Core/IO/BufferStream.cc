#include "BufferStream.hh"

#include "Core/Memory/MemoryUtils.hh"

namespace lr
{
    void BaseBufferStream::Seek(u8 seekFlag, u32 position)
    {
        ZoneScoped;

        switch (seekFlag)
        {
            case SEEK_END: m_DataOffset = m_DataLen - position; break;
            case SEEK_SET: m_DataOffset = position; break;
            case SEEK_CUR: m_DataOffset += position; break;
            default: break;
        }

        if (m_DataOffset < 0)
            m_DataOffset = 0;
        else if (m_DataOffset > m_DataLen)
            m_DataOffset = m_DataLen;
    }

    void BaseBufferStream::StartOver()
    {
        ZoneScoped;

        m_DataOffset = 0;
    }

    void *BaseBufferStream::Get(u32 dataLen, u32 count)
    {
        ZoneScoped;

        void *pData = m_pData + m_DataOffset;
        m_DataOffset += dataLen * count;

        return pData;
    }

    void *BaseBufferStream::GetNew(u32 dataLen, u32 count)
    {
        ZoneScoped;

        void *pData = malloc(dataLen * count);
        memcpy(pData, m_pData + m_DataOffset, dataLen * count);
        m_DataOffset += dataLen * count;

        return pData;
    }

    eastl::string_view BaseBufferStream::GetString(u32 dataSize)
    {
        ZoneScoped;

        char *data = (char *)(m_pData + m_DataOffset);
        m_DataOffset += dataSize;

        return eastl::string_view(data, dataSize);
    }

    const u8 *BaseBufferStream::GetData()
    {
        ZoneScoped;

        return m_pData;
    }

    const u8 *BaseBufferStream::GetOffsetPtr()
    {
        ZoneScoped;

        return m_pData + m_DataOffset;
    }

    const u32 BaseBufferStream::GetLength()
    {
        ZoneScoped;

        return m_DataLen;
    }

    const u32 BaseBufferStream::GetOffset()
    {
        ZoneScoped;

        return m_DataOffset;
    }

    ////////////////////////////////

    BufferReadStream::BufferReadStream(void *pData, u32 dataLen)
    {
        ZoneScoped;

        m_pData = (u8 *)pData;
        m_DataLen = dataLen;
    }

    BufferReadStream::BufferReadStream(FileStream &fileStream)
    {
        ZoneScoped;

        m_DataLen = fileStream.Size();
        m_pData = fileStream.ReadPtr<u8>(m_DataLen);
    }

    ////////////////////////////////

    BufferWriteStream::BufferWriteStream(u32 allocLen)
    {
        ZoneScoped;

        Reset(allocLen);
    }

    BufferWriteStream::BufferWriteStream(void *pData, u32 dataLen)
    {
        ZoneScoped;

        Reset((u8 *)pData, dataLen);
    }

    BufferWriteStream::~BufferWriteStream()
    {
        ZoneScoped;

        SAFE_FREE(m_pData);
    }

    void BufferWriteStream::Reset()
    {
        ZoneScoped;

        SAFE_FREE(m_pData);
        m_DataLen = 0;

        StartOver();
    }

    void BufferWriteStream::Reset(u32 dataLen)
    {
        ZoneScoped;

        SAFE_FREE(m_pData);
        m_DataLen = 0;

        AssignZero(dataLen);
        StartOver();
    }

    void BufferWriteStream::Reset(u8 *pData, u32 dataLen)
    {
        ZoneScoped;

        SAFE_FREE(m_pData);
        m_DataLen = 0;

        Expand(dataLen);
        Assign(pData, dataLen);
        StartOver();
    }

    void BufferWriteStream::Reset(BufferReadStream &stream)
    {
        ZoneScoped;

        SAFE_FREE(m_pData);
        m_DataLen = 0;

        Expand(stream.GetLength());
        Assign((u8 *)stream.GetData(), m_DataLen);

        StartOver();
    }

    void BufferWriteStream::Expand(u32 len)
    {
        ZoneScoped;

        m_DataLen += len;
        m_pData = (u8 *)realloc(m_pData, m_DataLen);
        Memory::ZeroMem((m_pData + m_DataOffset), len);
    }

    void BufferWriteStream::Assign(void *pData, u32 dataLen, u32 count)
    {
        ZoneScoped;

        SmartAlloc(dataLen * count);

        assert(m_pData != nullptr);            // Our data has to be valid
        assert(m_DataLen > 0);                 // Our data has to be allocated
        assert(pData != nullptr);              // Data has to be vaild
        assert(m_DataLen >= dataLen * count);  // Input data cannot be larger than data we have

        memcpy(m_pData + m_DataOffset, pData, dataLen * count);
        m_DataOffset += dataLen * count;
    }

    void BufferWriteStream::AssignZero(u32 dataSize)
    {
        ZoneScoped;

        SmartAlloc(dataSize);

        memset(m_pData + m_DataOffset, 0, dataSize);
        m_DataOffset += dataSize;
    }

    void BufferWriteStream::AssignString(const eastl::string &val)
    {
        ZoneScoped;

        Assign((void *)val.data(), val.length());
    }

    void BufferWriteStream::AssignString(eastl::string_view val)
    {
        ZoneScoped;

        Assign((void *)val.data(), val.length());
    }

    void BufferWriteStream::SmartAlloc(u32 insertedSize)
    {
        ZoneScoped;

        u32 expectedSize = m_DataOffset + insertedSize;
        if (expectedSize > m_DataLen)
            Expand(expectedSize - m_DataLen);
    }

}  // namespace lr
