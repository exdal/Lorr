// Created on Tuesday July 19th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>

typedef unsigned long DWORD;
typedef const wchar_t *LPCWSTR;
typedef void *HANDLE;
typedef struct HINSTANCE__ *HINSTANCE;
typedef struct HWND__ *HWND;
typedef struct HMONITOR__ *HMONITOR;
typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;

#include <vulkan/vulkan_win32.h>

#define _VK_FUNCTION_NAMES                                          \
    _VK_DEFINE_FUNCTION(vkCreateInstance);                          \
    _VK_DEFINE_FUNCTION(vkGetInstanceProcAddr);                     \
    _VK_DEFINE_FUNCTION(vkGetDeviceProcAddr);                       \
    _VK_DEFINE_FUNCTION(vkEnumerateInstanceExtensionProperties);    \
    _VK_DEFINE_FUNCTION(vkEnumerateInstanceLayerProperties);        \
    _VK_DEFINE_FUNCTION(vkEnumerateInstanceVersion);                \
    _VK_DEFINE_FUNCTION(vkResetCommandBuffer);                      \
    _VK_DEFINE_FUNCTION(vkCreateWin32SurfaceKHR);                   \
    _VK_DEFINE_FUNCTION(vkEnumeratePhysicalDevices);                \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceProperties);             \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceFeatures);               \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);  \
    _VK_DEFINE_FUNCTION(vkCreateDevice);                            \
    _VK_DEFINE_FUNCTION(vkGetDeviceQueue);                          \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);      \
    _VK_DEFINE_FUNCTION(vkEnumerateDeviceExtensionProperties);      \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR); \
    _VK_DEFINE_FUNCTION(vkCreateSwapchainKHR);                      \
    _VK_DEFINE_FUNCTION(vkCreateDescriptorPool);                    \
    _VK_DEFINE_FUNCTION(vkAllocateDescriptorSets);                  \
    _VK_DEFINE_FUNCTION(vkFreeDescriptorSets);                      \
    _VK_DEFINE_FUNCTION(vkGetSwapchainImagesKHR);                   \
    _VK_DEFINE_FUNCTION(vkDestroyImage);                            \
    _VK_DEFINE_FUNCTION(vkDestroyImageView);                        \
    _VK_DEFINE_FUNCTION(vkCreateImage);                             \
    _VK_DEFINE_FUNCTION(vkCreateImageView);                         \
    _VK_DEFINE_FUNCTION(vkCreateFramebuffer);                       \
    _VK_DEFINE_FUNCTION(vkAcquireNextImageKHR);                     \
    _VK_DEFINE_FUNCTION(vkCreateSemaphore);                         \
    _VK_DEFINE_FUNCTION(vkCreateFence);                             \
    _VK_DEFINE_FUNCTION(vkWaitForFences);                           \
    _VK_DEFINE_FUNCTION(vkResetFences);                             \
    _VK_DEFINE_FUNCTION(vkQueueSubmit);                             \
    _VK_DEFINE_FUNCTION(vkQueuePresentKHR);                         \
    _VK_DEFINE_FUNCTION(vkCreateCommandPool);                       \
    _VK_DEFINE_FUNCTION(vkAllocateCommandBuffers);                  \
    _VK_DEFINE_FUNCTION(vkBeginCommandBuffer);                      \
    _VK_DEFINE_FUNCTION(vkEndCommandBuffer);                        \
    _VK_DEFINE_FUNCTION(vkCmdBeginRendering);                       \
    _VK_DEFINE_FUNCTION(vkCmdEndRendering);                         \
    _VK_DEFINE_FUNCTION(vkQueueWaitIdle);                           \
    _VK_DEFINE_FUNCTION(vkCmdPipelineBarrier);                      \
    _VK_DEFINE_FUNCTION(vkResetCommandPool);                        \
    _VK_DEFINE_FUNCTION(vkFreeCommandBuffers);                      \
    _VK_DEFINE_FUNCTION(vkCmdClearColorImage);                      \
    _VK_DEFINE_FUNCTION(vkWaitSemaphores);                          \
    _VK_DEFINE_FUNCTION(vkSignalSemaphore);                         \
    _VK_DEFINE_FUNCTION(vkQueueSubmit2);                            \
    _VK_DEFINE_FUNCTION(vkCreatePipelineCache);                     \
    _VK_DEFINE_FUNCTION(vkCreateGraphicsPipelines);                 \
    _VK_DEFINE_FUNCTION(vkCreateShaderModule);                      \
    _VK_DEFINE_FUNCTION(vkCreateDescriptorSetLayout);               \
    _VK_DEFINE_FUNCTION(vkCreatePipelineLayout);                    \
    _VK_DEFINE_FUNCTION(vkCmdBindPipeline);                         \
    _VK_DEFINE_FUNCTION(vkCmdBindDescriptorSets);                   \
    _VK_DEFINE_FUNCTION(vkCmdBindVertexBuffers);                    \
    _VK_DEFINE_FUNCTION(vkCmdBindIndexBuffer);                      \
    _VK_DEFINE_FUNCTION(vkCmdDraw);                                 \
    _VK_DEFINE_FUNCTION(vkCmdDrawIndexed);                          \
    _VK_DEFINE_FUNCTION(vkCmdDrawIndirect);                         \
    _VK_DEFINE_FUNCTION(vkCmdDrawIndexedIndirect);                  \
    _VK_DEFINE_FUNCTION(vkCmdDispatch);                             \
    _VK_DEFINE_FUNCTION(vkCmdDispatchIndirect);                     \
    _VK_DEFINE_FUNCTION(vkCmdCopyBuffer);                           \
    _VK_DEFINE_FUNCTION(vkCmdCopyImage);                            \
    _VK_DEFINE_FUNCTION(vkCreateBuffer);                            \
    _VK_DEFINE_FUNCTION(vkAllocateMemory);                          \
    _VK_DEFINE_FUNCTION(vkBindBufferMemory);                        \
    _VK_DEFINE_FUNCTION(vkGetBufferMemoryRequirements);             \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceMemoryProperties);       \
    _VK_DEFINE_FUNCTION(vkDestroyBuffer);                           \
    _VK_DEFINE_FUNCTION(vkFreeMemory);                              \
    _VK_DEFINE_FUNCTION(vkMapMemory);                               \
    _VK_DEFINE_FUNCTION(vkUnmapMemory);                             \
    _VK_DEFINE_FUNCTION(vkGetImageMemoryRequirements);              \
    _VK_DEFINE_FUNCTION(vkBindImageMemory);                         \
    _VK_DEFINE_FUNCTION(vkUpdateDescriptorSets);                    \
    _VK_DEFINE_FUNCTION(vkDestroyFramebuffer);                      \
    _VK_DEFINE_FUNCTION(vkDestroyFence);                            \
    _VK_DEFINE_FUNCTION(vkDestroySemaphore);                        \
    _VK_DEFINE_FUNCTION(vkDestroySwapchainKHR);                     \
    _VK_DEFINE_FUNCTION(vkDeviceWaitIdle);                          \
    _VK_DEFINE_FUNCTION(vkCmdSetViewport);                          \
    _VK_DEFINE_FUNCTION(vkCmdSetScissor);                           \
    _VK_DEFINE_FUNCTION(vkDestroyShaderModule);                     \
    _VK_DEFINE_FUNCTION(vkGetFenceStatus);                          \
    _VK_DEFINE_FUNCTION(vkCmdCopyBufferToImage);                    \
    _VK_DEFINE_FUNCTION(vkCreateSampler);                           \
    _VK_DEFINE_FUNCTION(vkDestroySampler);                          \
    _VK_DEFINE_FUNCTION(vkCmdSetPrimitiveTopology);                 \
    _VK_DEFINE_FUNCTION(vkCmdPushConstants);                        \
    _VK_DEFINE_FUNCTION(vkCmdPipelineBarrier2);                     \
    _VK_DEFINE_FUNCTION(vkCreateComputePipelines);                  \
    _VK_DEFINE_FUNCTION(vkDestroyDescriptorSetLayout);              \
    _VK_DEFINE_FUNCTION(vkGetSemaphoreCounterValue);                \
    _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceProperties2);            \
    _VK_DEFINE_FUNCTION(vkGetBufferDeviceAddress);                  \
    _VK_DEFINE_FUNCTION(vkCreateQueryPool);                         \
    _VK_DEFINE_FUNCTION(vkResetQueryPool);                          \
    _VK_DEFINE_FUNCTION(vkDestroyQueryPool);                        \
    _VK_DEFINE_FUNCTION(vkCmdWriteTimestamp);                       \
    _VK_DEFINE_FUNCTION(vkCmdResetQueryPool);                       \
    _VK_DEFINE_FUNCTION(vkGetQueryPoolResults);

#define _VK_FUNCTION_NAMES_DEVICE                                      \
    _VK_DEFINE_FUNCTION(vkCmdBindDescriptorBufferEmbeddedSamplersEXT); \
    _VK_DEFINE_FUNCTION(vkCmdBindDescriptorBuffersEXT);                \
    _VK_DEFINE_FUNCTION(vkCmdSetDescriptorBufferOffsetsEXT);           \
    _VK_DEFINE_FUNCTION(vkGetBufferOpaqueCaptureDescriptorDataEXT);    \
    _VK_DEFINE_FUNCTION(vkGetDescriptorEXT);                           \
    _VK_DEFINE_FUNCTION(vkGetDescriptorSetLayoutBindingOffsetEXT);     \
    _VK_DEFINE_FUNCTION(vkGetDescriptorSetLayoutSizeEXT);              \
    _VK_DEFINE_FUNCTION(vkGetImageOpaqueCaptureDescriptorDataEXT);     \
    _VK_DEFINE_FUNCTION(vkGetImageViewOpaqueCaptureDescriptorDataEXT); \
    _VK_DEFINE_FUNCTION(vkGetSamplerOpaqueCaptureDescriptorDataEXT);   \
    _VK_DEFINE_FUNCTION(vkGetCalibratedTimestampsEXT);

#define _VK_FUNCTION_NAMES_INSTANCE _VK_DEFINE_FUNCTION(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT);
#define _VK_FUNCTION_NAMES_INSTANCE_DEBUG _VK_DEFINE_FUNCTION(vkSetDebugUtilsObjectNameEXT);

#define _VK_DEFINE_FUNCTION(_name) extern PFN_##_name _name;
_VK_FUNCTION_NAMES
_VK_FUNCTION_NAMES_DEVICE
_VK_FUNCTION_NAMES_INSTANCE
_VK_FUNCTION_NAMES_INSTANCE_DEBUG
#undef _VK_DEFINE_FUNCTION

namespace lr::Graphics::VK
{
void *LoadVulkan();
bool LoadVulkanInstance(VkInstance pInstance);
bool LoadVulkanDevice(VkDevice pDevice);
}