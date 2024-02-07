#include "Instance.hh"

#include <EASTL/scoped_array.h>
#include <VkBootstrap.h>

#include "Device.hh"
#include "Vulkan.hh"

#define VKFN_FUNCTION(_name) PFN_##_name _name
VKFN_INSTANCE_FUNCTIONS
VKFN_PHYSICAL_DEVICE_FUNCTIONS
VKFN_LOGICAL_DEVICE_FUNCTIONS
VKFN_COMMAND_BUFFER_FUNCTIONS
VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS
#undef VKFN_FUNCTION

namespace lr::Graphics
{
static vkb::Instance vkb_instance = {};

APIResult Instance::init(const InstanceDesc &desc)
{
    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(desc.m_app_name.data());
    instance_builder.set_engine_name(desc.m_engine_name.data());
    instance_builder.set_engine_version(desc.m_engine_version);
    instance_builder.enable_validation_layers(false);  // use vkconfig ui...
    instance_builder.request_validation_layers(false);
    instance_builder.enable_extensions({
        "VK_KHR_surface", "VK_KHR_get_physical_device_properties2", "VK_KHR_win32_surface",
#if _DEBUG
            //! Debug extensions, always put it to bottom
            "VK_EXT_debug_utils", "VK_EXT_debug_report",
#endif
    });
    instance_builder.require_api_version(1, 3, 0);
    auto build_result = instance_builder.build();
    if (!build_result)
    {
        LOG_ERROR("Failed to create Vulkan Instance!");
        return APIResult::InitFailed;
    }
    vkb_instance = build_result.value();
    m_handle = vkb_instance.instance;

#define VKFN_FUNCTION(_name)                                                      \
    _name = (PFN_##_name)vkb_instance.fp_vkGetInstanceProcAddr(m_handle, #_name); \
    if (_name == nullptr)                                                         \
        LOG_TRACE("Failed to load Vulkan function '{}'.", #_name);

    VKFN_INSTANCE_FUNCTIONS
    VKFN_PHYSICAL_DEVICE_FUNCTIONS
#if TRACY_ENABLE
    VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
#endif
#undef VKFN_FUNCTION

    return APIResult::Success;
}

APIResult Instance::create_devce(Device *device)
{
    ZoneScoped;

    vkb::PhysicalDeviceSelector physical_device_selector(vkb_instance);
    physical_device_selector.defer_surface_initialization();
    physical_device_selector.set_minimum_version(1, 3);
    physical_device_selector.set_required_features_13({
        .synchronization2 = true,
        .dynamicRendering = true,
    });
    physical_device_selector.set_required_features_12({
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .shaderStorageBufferArrayNonUniformIndexing = true,
        .descriptorBindingUpdateUnusedWhilePending = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .timelineSemaphore = true,
        .bufferDeviceAddress = true,
    });
    physical_device_selector.set_required_features({
        .vertexPipelineStoresAndAtomics = true,
        .fragmentStoresAndAtomics = true,
        .shaderInt64 = true,
    });
    physical_device_selector.add_required_extensions({
        "VK_KHR_swapchain",
#if TRACY_ENABLE
            "VK_EXT_calibrated_timestamps",
#endif
    });
    auto physical_device_result = physical_device_selector.select();
    if (!physical_device_result)
    {
        LOG_ERROR("Failed to select Vulkan Physical Device!");
        return static_cast<APIResult>(physical_device_result.vk_result());
    }
    auto &selected_physical_device = physical_device_result.value();
    DeviceFeature features = {};

    // if (selected_physical_device.enable_extension_if_present("VK_EXT_descriptor_buffer"))
    //     features |= DeviceFeature::DescriptorBuffer;
    if (selected_physical_device.enable_extension_if_present("VK_EXT_memory_budget"))
        features |= DeviceFeature::MemoryBudget;

    vkb::DeviceBuilder device_builder(selected_physical_device);

    VkPhysicalDeviceDescriptorBufferFeaturesEXT kDesciptorBufferFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = nullptr,
        .descriptorBuffer = true,
        .descriptorBufferImageLayoutIgnored = true,
        .descriptorBufferPushDescriptors = true,
    };
    if (features & DeviceFeature::DescriptorBuffer)
        device_builder.add_pNext(&kDesciptorBufferFeatures);

    auto device_result = device_builder.build();
    if (!device_result)
    {
        LOG_ERROR("Failed to select Vulkan Device!");
        return static_cast<APIResult>(device_result.vk_result());
    }

    VkPhysicalDevice physical_device = selected_physical_device.physical_device;
    VkDevice logical_device = device_result.value();

    // THESE ARE REQUIRED FUNCTIONS, THESE MUST BE AVAILABLE!!!
#define VKFN_FUNCTION(_name)                                                          \
    _name = (PFN_##_name)vkb_instance.fp_vkGetDeviceProcAddr(logical_device, #_name); \
    if (_name == nullptr)                                                             \
    {                                                                                 \
        LOG_ERROR("Failed to load Vulkan function '{}'.", #_name);                    \
        return APIResult::ExtNotPresent;                                              \
    }

    VKFN_LOGICAL_DEVICE_FUNCTIONS
    VKFN_COMMAND_BUFFER_FUNCTIONS
#if _DEBUG
    VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS
#endif
#if TRACY_ENABLE
    VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS
#endif
#undef VKFN_FUNCTION

    // OPTIONAL FUNCTIONS, MAINLY THERE IS AN ALTERNATIVE VERSION(EXTENSION) FOR THEM.
    // EXTENSIONS MUST BE CHECKED BEFORE LOADING
#define VKFN_FUNCTION(_name) _name = (PFN_##_name)vkb_instance.fp_vkGetDeviceProcAddr(logical_device, #_name);
    VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
#undef VKFN_FUNCTION

    eastl::array<u32, (usize)CommandType::Count> queue_indexes = {};
    for (u32 i = 0; i < queue_indexes.size(); i++)
        queue_indexes[i] = device_result->get_queue_index(static_cast<vkb::QueueType>(i + 1)).value();

    device->init(logical_device, physical_device, m_handle, queue_indexes);

    return APIResult::Success;
}

}  // namespace lr::Graphics
