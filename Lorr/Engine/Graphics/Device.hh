// Created on Sunday March 19th 2023 by exdal
// Last modified on Wednesday June 21st 2023 by exdal

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

struct DeviceMemory : APIObject<VK_OBJECT_TYPE_DEVICE_MEMORY>
{
    virtual u64 Allocate(u64 dataSize, u64 alignment, void *pAllocatorData) = 0;
    virtual void Free(void *pAllocatorData) = 0;

    VkDeviceMemory m_pHandle = VK_NULL_HANDLE;
};

struct TLSFDeviceMemory : DeviceMemory
{
    u64 Allocate(u64 dataSize, u64 alignment, void *pAllocatorData) override
    {
        ZoneScoped;

        Memory::TLSFBlockID blockID = m_Allocator.Allocate(dataSize, alignment);
        pAllocatorData = &blockID;
        return m_Allocator.GetBlockData(blockID)->m_Offset;
    }

    void Free(void *pAllocatorData) override
    {
        ZoneScoped;

        m_Allocator.Free(*(Memory::TLSFBlockID *)pAllocatorData);
    }

    Memory::TLSFAllocatorView m_Allocator = {};
};

struct LinearDeviceMemory : DeviceMemory
{
    u64 Allocate(u64 dataSize, u64 alignment, void *pAllocatorData) override
    {
        ZoneScoped;

        return m_Allocator.Allocate(dataSize, alignment);
    }

    void Free(void *pAllocatorData) override
    {
        ZoneScoped;

        m_Allocator.Free();
    }

    Memory::LinearAllocatorView m_Allocator = {};
};

struct ResourceAllocatorDesc
{
    u32 m_MaxTLSFAllocations = 0;
    u32 m_DescriptorMem = 0;
    u32 m_BufferLinearMem = 0;
    u32 m_BufferTLSFMem = 0;
    u32 m_BufferTLSFHostMem = 0;
    u32 m_BufferFrametimeMem = 0;
    u32 m_ImageTLSFMem = 0;
};

struct ResourceAllocators
{
    LinearDeviceMemory *m_pDescriptor = nullptr;
    LinearDeviceMemory *m_pBufferLinear = nullptr;
    TLSFDeviceMemory *m_pBufferTLSF = nullptr;
    TLSFDeviceMemory *m_pBufferTLSFHost = nullptr;
    LinearDeviceMemory *m_ppBufferFrametime[LR_MAX_FRAME_COUNT] = {};
    TLSFDeviceMemory *m_pImageTLSF = nullptr;

    DeviceMemory *GetDeviceMemory(ResourceAllocator allocator);
};

}  // namespace lr::Graphics