#include "Instance.hh"

#include <EASTL/scoped_array.h>
#include <VkBootstrap.h>

#include "Window/Win32/Win32Window.hh"

#include "Device.hh"
#include "PhysicalDevice.hh"
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
APIResult Instance::init(InstanceDesc *desc)
{
    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(desc->m_app_name.data());
    instance_builder.set_engine_name(desc->m_engine_name.data());
    instance_builder.set_engine_version(desc->m_engine_version);
    instance_builder.enable_validation_layers(false);  // use vkconfig ui...
    instance_builder.request_validation_layers(false);
    instance_builder.enable_extensions({
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_win32_surface",
#if _DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report",
#endif
    });
    instance_builder.require_api_version(1, 3, 0);
    auto build_result = instance_builder.build();
    if (!build_result)
    {
        LOG_ERROR("Failed to create Vulkan Instance!");
        return static_cast<APIResult>(build_result.vk_result());
    }
    auto &instance = build_result.value();
    m_handle = instance.instance;

#define VKFN_FUNCTION(_name)                                                  \
    _name = (PFN_##_name)instance.fp_vkGetInstanceProcAddr(m_handle, #_name); \
    if (_name == nullptr)                                                     \
        LOG_TRACE("Failed to load Vulkan function '{}'.", #_name);

    VKFN_INSTANCE_FUNCTIONS
    VKFN_PHYSICAL_DEVICE_FUNCTIONS
#if TRACY_ENABLE
    VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
#endif
#undef VKFN_FUNCTION

    VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = static_cast<HINSTANCE>(desc->m_window->m_instance),
        .hwnd = static_cast<HWND>(desc->m_window->m_handle),
    };
    auto result = static_cast<APIResult>(vkCreateWin32SurfaceKHR(m_handle, &surface_info, nullptr, &m_surface));
    if (result != APIResult::Success)
    {
        LOG_ERROR("Failed to create Win32 Surface.");
        return result;
    }

    vkb::PhysicalDeviceSelector physical_device_selector(instance);
    physical_device_selector.set_surface(m_surface);
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
    });
    physical_device_selector.set_required_features({
        .vertexPipelineStoresAndAtomics = true,
        .fragmentStoresAndAtomics = true,
        .shaderInt64 = true,
    });
    physical_device_selector.add_required_extensions({ "VK_KHR_swapchain" });
    auto physical_device_result = physical_device_selector.select();
    if (!physical_device_result)
    {
        LOG_ERROR("Failed to select Vulkan Physical Device!");
        return static_cast<APIResult>(physical_device_result.vk_result());
    }
    m_physical_device = physical_device_result->physical_device;
    auto &selected_physical_device = physical_device_result.value();

    // if (selected_physical_device.enable_extension_if_present("VK_EXT_descriptor_buffer"))
    //     m_supported_features |= DeviceFeature::DescriptorBuffer;
    if (selected_physical_device.enable_extension_if_present("VK_EXT_memory_budget"))
        m_supported_features |= DeviceFeature::MemoryBudget;

    vkb::DeviceBuilder device_builder(selected_physical_device);

     VkPhysicalDeviceDescriptorBufferFeaturesEXT kDesciptorBufferFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = nullptr,
        .descriptorBuffer = true,
        .descriptorBufferImageLayoutIgnored = true,
        .descriptorBufferPushDescriptors = true,
    };
    if (m_supported_features & DeviceFeature::DescriptorBuffer)
        device_builder.add_pNext(&kDesciptorBufferFeatures);

    auto device_result = device_builder.build();
    if (!device_result)
    {
        LOG_ERROR("Failed to select Vulkan Device!");
        return static_cast<APIResult>(device_result.vk_result());
    }
    m_device = device_result.value();
    m_present_queue_index = device_result->get_queue_index(vkb::QueueType::present).value();
    for (u32 i = 0; i < m_queue_indexes.size(); i++)
        m_queue_indexes[i] = device_result->get_queue_index(static_cast<vkb::QueueType>(i + 1)).value();

        // THESE ARE REQUIRED FUNCTIONS, THESE MUST BE AVAILABLE!!!
#define VKFN_FUNCTION(_name)                                                \
    _name = (PFN_##_name)instance.fp_vkGetDeviceProcAddr(m_device, #_name); \
    if (_name == nullptr)                                                   \
    {                                                                       \
        LOG_ERROR("Failed to load Vulkan function '{}'.", #_name);          \
        return APIResult::ExtNotPresent;                                    \
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
#define VKFN_FUNCTION(_name) _name = (PFN_##_name)instance.fp_vkGetDeviceProcAddr(m_device, #_name);
    VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
#undef VKFN_FUNCTION

    return APIResult::Success;
}

APIResult Instance::get_win32_surface(Surface *surface)
{
    validate_handle(surface);

    surface->m_handle = m_surface;
    return m_surface ? APIResult::Success : APIResult::InitFailed;
}

APIResult Instance::get_physical_device(PhysicalDevice *physical_device)
{
    validate_handle(physical_device);

    physical_device->init(m_physical_device, m_supported_features);
    return m_physical_device ? APIResult::Success : APIResult::InitFailed;
}

APIResult Instance::get_logical_device(Device *device, PhysicalDevice *physical_device)
{
    device->init(m_device, physical_device, m_queue_indexes);

    return m_device ? APIResult::Success : APIResult::InitFailed;
}

}  // namespace lr::Graphics