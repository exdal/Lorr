#include "Resource.hh"

namespace lr::Graphics {
ImageSubresourceRange::ImageSubresourceRange(ImageSubresourceInfo info)
    : VkImageSubresourceRange()
{
    this->aspectMask = static_cast<VkImageAspectFlags>(info.m_aspect_mask);
    this->baseMipLevel = info.m_base_mip;
    this->levelCount = info.m_mip_count;
    this->baseArrayLayer = info.m_base_slice;
    this->layerCount = info.m_slice_count;
}
}  // namespace lr::Graphics
