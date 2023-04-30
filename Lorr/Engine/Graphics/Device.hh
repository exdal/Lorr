//
// Created on Sunday 19th March 2023 by exdal
//

#pragma once

#include "Window/BaseWindow.hh"

#include "APIAllocator.hh"
#include "CommandList.hh"

namespace lr::Graphics
{
struct Surface;
struct PhysicalDevice : APIObject<VK_OBJECT_TYPE_PHYSICAL_DEVICE>
{
    void Init(eastl::span<VkPhysicalDevice> devices);
    void InitQueueFamilies(Surface *pSurface);
    VkDevice GetLogicalDevice();
    u32 GetHeapIndex(VkMemoryPropertyFlags flags);
    u32 GetQueueIndex(CommandType type);
    u64 GetDescriptorBufferAlignment()
    {
        return m_FeatureDescriptorBufferProps.descriptorBufferOffsetAlignment;
    }

    u32 SelectQueue(Surface *pSurface, VkQueueFlags desiredQueue, bool requirePresent, bool selectBest);

    u32 m_PresentQueueIndex = 0;
    eastl::array<VkDeviceQueueCreateInfo, (u32)CommandType::Count> m_QueueInfos;
    eastl::vector<VkQueueFamilyProperties> m_QueueProperties;
    eastl::vector<u32> m_SelectedQueueIndices;
    eastl::vector<VkExtensionProperties> m_DeviceExtensions;

    /// DEVICE FEATURES ///
    VkPhysicalDeviceProperties m_FeatureDeviceProps = {};
    VkPhysicalDeviceMemoryProperties m_FeatureMemoryProps = {};
    // VK_EXT_descriptor_buffer
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_FeatureDescriptorBufferProps = {};

    VkPhysicalDevice m_pHandle = VK_NULL_HANDLE;
};

struct Surface : APIObject<VK_OBJECT_TYPE_SURFACE_KHR>
{
    void Init(BaseWindow *pWindow, VkInstance pInstance, PhysicalDevice *pPhysicalDevice);
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