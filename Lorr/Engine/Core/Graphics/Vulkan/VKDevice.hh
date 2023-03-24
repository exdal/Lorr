//
// Created on Sunday 19th March 2023 by exdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

#include "Allocator.hh"
#include "VKCommandList.hh"
#include "Vulkan.hh"

namespace lr::Graphics
{
struct VKSurface;
struct VKPhysicalDevice : APIObject<VK_OBJECT_TYPE_PHYSICAL_DEVICE>
{
    void Init(eastl::span<VkPhysicalDevice> devices);
    void InitQueueFamilies(VKSurface *pSurface);
    VkDevice GetLogicalDevice();
    u32 GetHeapIndex(VkMemoryPropertyFlags flags);
    u32 GetQueueIndex(CommandListType type);

    u32 SelectQueue(VKSurface *pSurface, VkQueueFlags desiredQueue, bool requirePresent, bool selectBest);

    u32 m_PresentQueueIndex = 0;
    eastl::array<VkDeviceQueueCreateInfo, LR_COMMAND_LIST_TYPE_COUNT> m_QueueInfos;
    eastl::vector<VkQueueFamilyProperties> m_QueueProperties;
    eastl::vector<VkExtensionProperties> m_DeviceExtensions;

    VkPhysicalDeviceMemoryProperties m_MemoryInfo = {};

    VkPhysicalDevice m_pHandle = VK_NULL_HANDLE;
};

struct VKSurface : APIObject<VK_OBJECT_TYPE_SURFACE_KHR>
{
    void Init(BaseWindow *pWindow, VkInstance pInstance, VKPhysicalDevice *pPhysicalDevice);
    bool IsFormatSupported(VkFormat format, VkColorSpaceKHR &colorSpaceOut);
    bool IsPresentModeSupported(VkPresentModeKHR mode);

    eastl::vector<VkSurfaceFormatKHR> m_SurfaceFormats;
    eastl::vector<VkPresentModeKHR> m_PresentModes;

    VkCompositeAlphaFlagsKHR m_SupportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkSurfaceTransformFlagsKHR m_SupportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkSurfaceTransformFlagBitsKHR m_CurrentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    u32 m_MinImageCount = 0;
    u32 m_MaxImageCount = 0;
    XMUINT2 m_MinImageSize = {};
    XMUINT2 m_MaxImageSize = {};

    VkSurfaceKHR m_pHandle = VK_NULL_HANDLE;
};

}  // namespace lr::Graphics