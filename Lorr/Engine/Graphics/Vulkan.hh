#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <VkBootstrap.h>

#define VKFN_INSTANCE_FUNCTIONS                   \
    VKFN_FUNCTION(vkGetDeviceProcAddr);           \
    VKFN_FUNCTION(vkGetInstanceProcAddr);         \
    VKFN_FUNCTION(vkEnumeratePhysicalDevices);    \
    VKFN_FUNCTION(vkGetPhysicalDeviceProperties); \
    VKFN_FUNCTION(vkGetPhysicalDeviceProperties2);

#define VKFN_PHYSICAL_DEVICE_FUNCTIONS                        \
    VKFN_FUNCTION(vkGetPhysicalDeviceFeatures);               \
    VKFN_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);  \
    VKFN_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);      \
    VKFN_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
    VKFN_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
    VKFN_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR); \
    VKFN_FUNCTION(vkEnumerateDeviceExtensionProperties);      \
    VKFN_FUNCTION(vkGetPhysicalDeviceMemoryProperties);       \
    VKFN_FUNCTION(vkGetPhysicalDeviceMemoryProperties2);      \
    VKFN_FUNCTION(vkCreateDevice);

#define VKFN_LOGICAL_DEVICE_FUNCTIONS             \
    VKFN_FUNCTION(vkGetDeviceQueue);              \
    VKFN_FUNCTION(vkCreateSwapchainKHR);          \
    VKFN_FUNCTION(vkDestroySwapchainKHR);         \
    VKFN_FUNCTION(vkGetSwapchainImagesKHR);       \
    VKFN_FUNCTION(vkAcquireNextImageKHR);         \
    VKFN_FUNCTION(vkQueuePresentKHR);             \
    VKFN_FUNCTION(vkQueueWaitIdle);               \
    VKFN_FUNCTION(vkQueueSubmit2);                \
    VKFN_FUNCTION(vkCreateSemaphore);             \
    VKFN_FUNCTION(vkWaitSemaphores);              \
    VKFN_FUNCTION(vkSignalSemaphore);             \
    VKFN_FUNCTION(vkGetSemaphoreCounterValue);    \
    VKFN_FUNCTION(vkDestroySemaphore);            \
    VKFN_FUNCTION(vkCreateImage);                 \
    VKFN_FUNCTION(vkCreateImageView);             \
    VKFN_FUNCTION(vkDestroyImage);                \
    VKFN_FUNCTION(vkDestroyImageView);            \
    VKFN_FUNCTION(vkBindImageMemory);             \
    VKFN_FUNCTION(vkGetImageMemoryRequirements);  \
    VKFN_FUNCTION(vkCreateBuffer);                \
    VKFN_FUNCTION(vkDestroyBuffer);               \
    VKFN_FUNCTION(vkGetBufferDeviceAddress);      \
    VKFN_FUNCTION(vkBindBufferMemory);            \
    VKFN_FUNCTION(vkGetBufferMemoryRequirements); \
    VKFN_FUNCTION(vkCreateSampler);               \
    VKFN_FUNCTION(vkDestroySampler);              \
    VKFN_FUNCTION(vkAllocateMemory);              \
    VKFN_FUNCTION(vkFreeMemory);                  \
    VKFN_FUNCTION(vkMapMemory);                   \
    VKFN_FUNCTION(vkUnmapMemory);                 \
    VKFN_FUNCTION(vkCreateDescriptorSetLayout);   \
    VKFN_FUNCTION(vkDestroyDescriptorSetLayout);  \
    VKFN_FUNCTION(vkCreateDescriptorPool);        \
    VKFN_FUNCTION(vkDestroyDescriptorPool);       \
    VKFN_FUNCTION(vkAllocateDescriptorSets);      \
    VKFN_FUNCTION(vkFreeDescriptorSets);          \
    VKFN_FUNCTION(vkUpdateDescriptorSets);        \
    VKFN_FUNCTION(vkCreateGraphicsPipelines);     \
    VKFN_FUNCTION(vkCreateComputePipelines);      \
    VKFN_FUNCTION(vkDestroyPipeline);             \
    VKFN_FUNCTION(vkCreatePipelineLayout);        \
    VKFN_FUNCTION(vkDestroyPipelineLayout);       \
    VKFN_FUNCTION(vkCreateShaderModule);          \
    VKFN_FUNCTION(vkDestroyShaderModule);         \
    VKFN_FUNCTION(vkCreateCommandPool);           \
    VKFN_FUNCTION(vkDestroyCommandPool);          \
    VKFN_FUNCTION(vkResetCommandPool);            \
    VKFN_FUNCTION(vkAllocateCommandBuffers);      \
    VKFN_FUNCTION(vkFreeCommandBuffers);          \
    VKFN_FUNCTION(vkBeginCommandBuffer);          \
    VKFN_FUNCTION(vkEndCommandBuffer);            \
    VKFN_FUNCTION(vkResetCommandBuffer);          \
    VKFN_FUNCTION(vkCreateQueryPool);             \
    VKFN_FUNCTION(vkDestroyQueryPool);            \
    VKFN_FUNCTION(vkResetQueryPool);              \
    VKFN_FUNCTION(vkGetQueryPoolResults);         \
    VKFN_FUNCTION(vkDeviceWaitIdle);

#define VKFN_COMMAND_BUFFER_FUNCTIONS         \
    VKFN_FUNCTION(vkCmdBeginRendering);       \
    VKFN_FUNCTION(vkCmdEndRendering);         \
    VKFN_FUNCTION(vkCmdPipelineBarrier);      \
    VKFN_FUNCTION(vkCmdClearColorImage);      \
    VKFN_FUNCTION(vkCmdBindPipeline);         \
    VKFN_FUNCTION(vkCmdBindDescriptorSets);   \
    VKFN_FUNCTION(vkCmdBindVertexBuffers);    \
    VKFN_FUNCTION(vkCmdBindIndexBuffer);      \
    VKFN_FUNCTION(vkCmdDraw);                 \
    VKFN_FUNCTION(vkCmdDrawIndexed);          \
    VKFN_FUNCTION(vkCmdDrawIndirect);         \
    VKFN_FUNCTION(vkCmdDrawIndexedIndirect);  \
    VKFN_FUNCTION(vkCmdDispatch);             \
    VKFN_FUNCTION(vkCmdDispatchIndirect);     \
    VKFN_FUNCTION(vkCmdCopyBuffer);           \
    VKFN_FUNCTION(vkCmdCopyImage);            \
    VKFN_FUNCTION(vkCmdSetViewport);          \
    VKFN_FUNCTION(vkCmdSetScissor);           \
    VKFN_FUNCTION(vkCmdCopyBufferToImage);    \
    VKFN_FUNCTION(vkCmdSetPrimitiveTopology); \
    VKFN_FUNCTION(vkCmdPushConstants);        \
    VKFN_FUNCTION(vkCmdPipelineBarrier2);     \
    VKFN_FUNCTION(vkCmdWriteTimestamp);       \
    VKFN_FUNCTION(vkCmdResetQueryPool);

#define VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS                     \
    VKFN_FUNCTION(vkCmdBindDescriptorBufferEmbeddedSamplersEXT); \
    VKFN_FUNCTION(vkCmdBindDescriptorBuffersEXT);                \
    VKFN_FUNCTION(vkCmdSetDescriptorBufferOffsetsEXT);           \
    VKFN_FUNCTION(vkGetBufferOpaqueCaptureDescriptorDataEXT);    \
    VKFN_FUNCTION(vkGetDescriptorEXT);                           \
    VKFN_FUNCTION(vkGetDescriptorSetLayoutBindingOffsetEXT);     \
    VKFN_FUNCTION(vkGetDescriptorSetLayoutSizeEXT);              \
    VKFN_FUNCTION(vkGetImageOpaqueCaptureDescriptorDataEXT);     \
    VKFN_FUNCTION(vkGetImageViewOpaqueCaptureDescriptorDataEXT); \
    VKFN_FUNCTION(vkGetSamplerOpaqueCaptureDescriptorDataEXT);

#define VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS VKFN_FUNCTION(vkGetCalibratedTimestampsEXT);

#define VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS \
    VKFN_FUNCTION(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT);

#define VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS VKFN_FUNCTION(vkSetDebugUtilsObjectNameEXT);

#define VKFN_FUNCTION(_name) extern PFN_##_name _name;
VKFN_INSTANCE_FUNCTIONS
VKFN_PHYSICAL_DEVICE_FUNCTIONS
VKFN_LOGICAL_DEVICE_FUNCTIONS
VKFN_COMMAND_BUFFER_FUNCTIONS
VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS
#undef VKFN_FUNCTION

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000
#include <vk_mem_alloc.h>

namespace lr::graphics::vulkan {
bool load_instance(VkInstance instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr);
bool load_device(VkDevice device, PFN_vkGetDeviceProcAddr get_device_proc_addr);
}  // namespace lr::graphics::vulkan
