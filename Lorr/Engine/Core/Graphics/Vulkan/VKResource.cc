#include "VKResource.hh"

#include "Core/Crypt/FNV.hh"

#include "VKContext.hh"

namespace lr::Graphics
{
DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Buffer *pBuffer, u32 offset, u32 dataSize)
{
    m_Binding = binding;
    m_Count = count;
    m_pBuffer = pBuffer;
    m_BufferDataOffset = offset;
    m_BufferDataSize = dataSize == ~0 ? pBuffer->m_DataLen : dataSize;
    m_Type = LR_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Image *pImage, ImageLayout layout)
{
    m_Binding = binding;
    m_Count = count;
    m_pImage = pImage;
    m_ImageLayout = layout;
    m_Type = LR_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
}

DescriptorWriteData::DescriptorWriteData(u32 binding, u32 count, Sampler *pSampler)
{
    m_Binding = binding;
    m_Count = count;
    m_pSampler = pSampler;
    m_Type = LR_DESCRIPTOR_TYPE_SAMPLER;
}

}  // namespace lr::Graphics