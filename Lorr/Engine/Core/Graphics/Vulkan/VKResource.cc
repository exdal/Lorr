#include "VKResource.hh"

#include "Core/Crypto/FNV.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Buffer *pBuffer, u32 offset, u32 dataSize)
    {
        m_Binding = binding;
        m_Count = count;
        m_pBuffer = pBuffer;
        m_BufferDataOffset = offset;
        m_BufferDataSize = dataSize == ~0 ? pBuffer->m_DataLen : dataSize;
    }

    DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Image *pImage, ImageLayout layout)
    {
        m_Binding = binding;
        m_Count = count;
        m_pImage = pImage;
        m_ImageLayout = layout;
    }

    DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Sampler *pSampler)
    {
        m_Binding = binding;
        m_Count = count;
        m_pSampler = pSampler;
    }

    DescriptorSetLayout *DescriptorLayoutCache::Get(eastl::span<DescriptorLayoutElement> elements, u64 &hashOut)
    {
        ZoneScoped;

        hashOut = Hash::FNV64((char *)elements.data(), elements.size_bytes());
        for (CachedLayout &layout : m_Layouts)
            if (hashOut == layout.m_Hash)
                return layout.m_pHandle;

        return nullptr;
    }

    void DescriptorLayoutCache::Add(DescriptorSetLayout *pLayout, u64 hash)
    {
        ZoneScoped;

        m_Layouts.push_back({ hash, pLayout });
    }

}  // namespace lr::Graphics