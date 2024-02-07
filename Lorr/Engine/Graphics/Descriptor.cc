#include "Descriptor.hh"

#include "Device.hh"
#include "Resource.hh"

namespace lr::Graphics {
DescriptorLayoutElement::DescriptorLayoutElement(
    u32 binding, DescriptorType type, ShaderStage stage, u32 max_descriptors, DescriptorBindingFlag flags)
{
    m_binding_info = {};
    m_binding_info.binding = binding;
    m_binding_info.descriptorType = static_cast<VkDescriptorType>(type);
    m_binding_info.stageFlags = static_cast<VkShaderStageFlags>(stage);
    m_binding_info.descriptorCount = max_descriptors;
    m_binding_info.pImmutableSamplers = nullptr;  // No need for static samplers, bindless good
    m_binding_flag = flags;
}

ImageDescriptorInfo::ImageDescriptorInfo(ImageView *image_view, ImageLayout layout)
    : VkDescriptorImageInfo(VK_NULL_HANDLE, image_view->m_handle, static_cast<VkImageLayout>(layout))
{
}

BufferDescriptorInfo::BufferDescriptorInfo(Buffer *buffer, u64 size, u64 offset)
    : VkDescriptorBufferInfo(buffer->m_handle, offset, size)
{
}

SamplerDescriptorInfo::SamplerDescriptorInfo(Sampler *sampler)
    : VkDescriptorImageInfo(sampler->m_handle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED)
{
}
/// `VkDescriptorDataEXT` is an union so var names such as `data.pUniformTexelBuffer`
/// do not matter, only thing that matters is type
DescriptorGetInfo::DescriptorGetInfo(Buffer *buffer, u64 data_size, DescriptorType type)
    : m_buffer_range(buffer->m_device_address, data_size),
      m_type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(ImageView *image_view, DescriptorType type)
    : m_image_view(image_view),
      m_type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(Sampler *sampler)
    : m_sampler(sampler),
      m_type(DescriptorType::Sampler)
{
}

DescriptorPoolSize::DescriptorPoolSize(DescriptorType type, u32 count)
    : VkDescriptorPoolSize()
{
    this->type = static_cast<VkDescriptorType>(type);
    this->descriptorCount = count;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, ImageDescriptorInfo &image_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pImageInfo = &image_info;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, BufferDescriptorInfo &buffer_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pBufferInfo = &buffer_info;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, SamplerDescriptorInfo &sampler_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pImageInfo = &sampler_info;
}

WriteDescriptorSet::WriteDescriptorSet(DescriptorSet *descriptor_set, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : VkWriteDescriptorSet(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr)
{
    this->dstSet = descriptor_set->m_handle;
    this->dstBinding = dst_binding;
    this->dstArrayElement = dst_element;
    this->descriptorCount = dst_count;
    this->descriptorType = static_cast<VkDescriptorType>(dst_type);
}

CopyDescriptorSet::CopyDescriptorSet(
    DescriptorSet *src, u32 src_binding, u32 src_element, DescriptorSet *dst, u32 dst_binding, u32 dst_element, u32 count)
    : VkCopyDescriptorSet(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr)
{
    this->srcSet = src->m_handle;
    this->srcBinding = src_binding;
    this->srcArrayElement = src_element;

    this->dstSet = dst->m_handle;
    this->dstBinding = dst_binding;
    this->dstArrayElement = dst_element;

    this->descriptorCount = count;
}

DescriptorManager &DescriptorManager::begin(Device *device, u32 frame_count)
{
    ZoneScoped;

    m_descriptor_set_ranges.clear();
    m_descriptor_sets.clear();
    m_arranged_sets.clear();

    m_frame_count = frame_count;
    m_device = device;

    return *this;
}

DescriptorManager &DescriptorManager::add_set(DescriptorLayoutElement element, u32 count, DescriptorSetID &result)
{
    ZoneScoped;
    assert(count != 0);

    usize descriptor_size = m_descriptor_sets.size();
    usize range_size = m_descriptor_set_ranges.size();

    m_descriptor_sets.resize(descriptor_size + count);
    m_descriptor_set_ranges.resize(range_size + 1);

    m_descriptor_set_ranges[range_size] = { element, descriptor_size, count };
    result = set_handle_val<DescriptorSetID>(range_size);

    return *this;
}

void DescriptorManager::build()
{
    ZoneScoped;

    // determine maximum pool sizes of each descriptors
    eastl::vector<DescriptorPoolSize> pool_sizes = {};
    u32 max_sets = 0;
    for (auto &range : m_descriptor_set_ranges) {
        pool_sizes.push_back({ range.element.get_type(), range.element.get_count() });
        max_sets += range.count;
    }

    m_device->create_descriptor_pool(&m_pool, max_sets, pool_sizes);

    // create sets linearly
    eastl::vector<DescriptorSetLayout> layouts = {};
    for (auto &range : m_descriptor_set_ranges) {
        eastl::span descriptor_sets = { m_descriptor_sets.begin() + range.start, range.count };

        // frame_count must always have same value among all ranges
        // a range cannot have count of 2 descriptors if the last range had 3
        assert(range.count == 1 || range.count == m_frame_count);

        DescriptorSetLayout layout = {};
        m_device->create_descriptor_set_layout(&layout, range.element, DescriptorSetLayoutFlag::None);

        for (DescriptorSet &set : descriptor_sets) {
            m_device->create_descriptor_set(&set, &layout, &m_pool);
        }

        layouts.push_back(layout);
    }

    // create pipeline layout per push constant size
    m_device->create_pipeline_layout(&m_pipeline_layouts[0], layouts, {});
    for (usize i = 1; i < m_pipeline_layouts.size(); i++) {
        PushConstantDesc push_constant_desc(ShaderStage::All, 0, i * sizeof(u32));
        m_device->create_pipeline_layout(&m_pipeline_layouts[i], layouts, push_constant_desc);
    }

    for (DescriptorSetLayout &layout : layouts) {
        m_device->delete_descriptor_set_layout(&layout);
    }

    // DESCRIPTOR SET REARRANGE
    // rearrange sets relative to frame
    for (u32 i = 0; i < m_frame_count; i++) {
        for (auto &range : m_descriptor_set_ranges) {
            u32 offset = range.start + i % range.count;
            DescriptorSet &set = m_descriptor_sets[offset];
            m_arranged_sets.push_back(set);
        }
    }
}

eastl::span<DescriptorSet> DescriptorManager::get_arranged_sets(u32 slice_offset)
{
    ZoneScoped;

    usize slice_size = get_slice_size();
    return { m_arranged_sets.begin() + (slice_offset * slice_size), slice_size };
}
}  // namespace lr::Graphics
