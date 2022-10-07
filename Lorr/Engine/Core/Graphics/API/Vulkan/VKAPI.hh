//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "IO/BufferStream.hh"
#include "IO/MemoryAllocator.hh"

#include "VKSym.hh"

#include "VKCommandQueue.hh"
#include "VKPipeline.hh"
#include "VKSwapChain.hh"

namespace lr::g
{
    /// -------------- RenderPasses --------------

    constexpr u32 kMaxSubpassCount = 8;
    constexpr u32 kMaxColorAttachmentCount = 8;

    // `AttachmentFlags` also helps us to select `VkSubpassDependency`.
    enum class AttachmentFlags
    {
        None = 0,
        AllowStencil = 1 << 0,
        PixelShaderVisible = 1 << 1,
        MemoryRead = 1 << 2,
        MemoryWrite = 1 << 3,
    };
    EnumFlags(AttachmentFlags);

    struct RenderPassAttachment
    {
        AttachmentFlags Flags = AttachmentFlags::None;
        ResourceFormat Format;
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;  // MSAA -> If > 1_BIT

        VkAttachmentLoadOp LoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp StoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp StencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp StencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // We usually don't care about initial layout
        VkImageLayout FinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct RenderPassSubpassDesc  // :skull:
    {
        // `8` is the maximum attachments per `RenderPass` we support.
        u32 ColorAttachmentCount = 0;
        RenderPassAttachment *ppColorAttachments[kMaxColorAttachmentCount];
        RenderPassAttachment *pDepthAttachment = nullptr;

        // If source subpass index is `0` and ProducerStage is `0`,
        // we take present command queue's wait stage as `ProducerStage`
        VkPipelineStageFlagBits2 ProducerStage = 0;  // AKA `srcStage`
        VkPipelineStageFlagBits2 ConsumerStage = 0;  // AKA `dstStage`

        VkAccessFlagBits2 ProducerAccess = 0;
        VkAccessFlagBits2 ConsumerAccess = 0;
    };

    // TODO: Currently, we don't support multiple subpasses
    struct RenderPassDesc
    {
        // u32 SubpassCount = 0;
        // RenderPassSubpassDesc * ppSubpasses[kMaxSubpassCount];
        RenderPassSubpassDesc *pSubpassDesc = nullptr;
    };

    /// ------------------------------------------ ///

    struct VKLinearMemoryAllocator
    {
        Memory::LinearMemoryAllocator Allocator;
        VKBuffer Buffer;
    };

    struct VKTLSFMemoryAllocator
    {
        Memory::TLSFMemoryAllocator Allocator;
        VKBuffer Buffer;
    };

    /// ------------------------------------------ ///

    enum class APIFlags : u32
    {
        None,

        VSync = 1 << 0,
    };
    EnumFlags(APIFlags);

    struct VKAPI
    {
        bool Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags);

        void InitCommandLists();
        void InitAllocators();

        /// COMMAND ///
        void CreateCommandQueue(VKCommandQueue *pHandle, CommandListType type);
        void CreateCommandAllocator(VKCommandAllocator *pHandle, CommandListType type);
        void CreateCommandList(VKCommandList *pHandle, CommandListType type);

        // TODO: VERY BAD NAMES!
        // Takes a list from pool
        VKCommandList *GetCommandList();
        // Returns command list of current frame
        VKCommandList *GetCurrentCommandList();

        void BeginCommandList(VKCommandList *pList);
        void EndCommandList(VKCommandList *pList);
        void ResetCommandAllocator(VKCommandAllocator *pAllocator);

        // Note: `vkQueueSubmit` is not thread-safe.
        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        void ExecuteCommandList(VKCommandList *pList);
        // Executes command list of current frame, it always performs a present operation.
        void ExecuteCommandQueue();

        VkFence CreateFence(bool signaled);
        VkSemaphore CreateSemaphore(u32 initialValue = 0, bool binary = true);

        /// RENDERPASS ///
        VkRenderPass CreateRenderPass(RenderPassDesc *pDesc);

        /// PIPELINE ///
        VkPipelineCache CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        void BeginPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo, VkRenderPass pRenderPass);
        void EndPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo,
                                  VKPipeline *pTargetHandle,
                                  const std::initializer_list<VKDescriptorSet *> &sets);

        /// SWAP CHAIN ///
        void Resize(u32 width, u32 height);
        void CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info);
        void GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages);

        void Frame();

        /// RESOURCE ///

        VkShaderModule CreateShaderModule(BufferReadStream &buf);
        void CreateDescriptorSetLayout(VKDescriptorSet *pSet, VKDescriptorSetDesc *pDesc);

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        VkDescriptorPool CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts);

        u32 GetMemoryTypeIndex(u32 setBits, VkMemoryPropertyFlags propFlags);
        void AllocateImageMemory(VKImage *pImage, AllocatorType allocatorType);
        void AllocateBufferMemory(VKBuffer *pBuffer, AllocatorType allocatorType);

        void CreateBuffer(VKBuffer *pHandle, BufferDesc *pDesc, BufferData *pData);
        void BindMemory(VKBuffer *pBuffer);
        void DeleteBuffer(VKBuffer *pHandle);
        void MapMemory(VKBuffer *pBuffer, void *&pData);
        void UnmapMemory(VKBuffer *pBuffer);

        void CreateImage(VKImage *pHandle, ImageDesc *pDesc, ImageData *pData);
        void CreateImageView(VKImage *pHandle);
        VkFramebuffer CreateFramebuffer(XMUINT2 size, u32 attachmentCount, VKImage *pAttachments, VkRenderPass &pRenderPass);
        void BindMemory(VKImage *pImage);
        void DeleteImage(VKImage *pImage);

        bool IsFormatSupported(VkFormat format, VkColorSpaceKHR *pColorSpaceOut);
        bool IsPresentModeSupported(VkPresentModeKHR format);
        void GetSurfaceCapabilities(VkSurfaceCapabilitiesKHR &capabilitiesOut);

        bool LoadVulkan();
        bool SetupInstance(HWND windowHandle);
        bool SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);
        bool SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);

        /// ------------------------------------------------------------- ///

        /// Type conversions
        static VkFormat ToVulkanFormat(ResourceFormat format);
        static VkFormat ToVulkanFormat(VertexAttribType format);
        static VkPrimitiveTopology ToVulkanTopology(PrimitiveType type);
        static VkCullModeFlags ToVulkanCullMode(CullMode mode);
        static VkDescriptorType ToVulkanDescriptorType(DescriptorType type);
        static VkImageUsageFlags ToVulkanImageUsage(ResourceType type);
        static VkBufferUsageFlagBits ToVulkanBufferUsage(BufferUsage usage);

        /// ------------------------------------------------------------- ///

        /// Renderer Handles
        VkInstance m_pInstance = nullptr;
        VkPhysicalDevice m_pPhysicalDevice = nullptr;
        VkSurfaceKHR m_pSurface = nullptr;
        VkDevice m_pDevice = nullptr;
        VKSwapChain m_SwapChain;

        /// Main Queues
        u32 m_AvailableQueueCount = 0;
        eastl::array<u32, 3> m_QueueIndexes;
        VKCommandQueue m_DirectQueue;
        eastl::array<VKCommandList, 3> m_ListsInFlight;

        /// Pools/Caches
        CommandListPool m_CommandListPool;
        VKPipelineManager m_PipelineManager;
        VkDescriptorPool m_pDescriptorPool = nullptr;

        VKLinearMemoryAllocator m_BufferAlloc_Linear;
        VKTLSFMemoryAllocator m_BufferAlloc_TLSF;
        VKTLSFMemoryAllocator m_ImageAlloc_TLSF;

        /// Native API
        VkPhysicalDeviceMemoryProperties m_MemoryProps;

        VkSurfaceFormatKHR *m_pValidSurfaceFormats = nullptr;
        u32 m_ValidSurfaceFormatCount = 0;

        VkPresentModeKHR *m_pValidPresentModes = nullptr;
        u32 m_ValidPresentModeCount = 0;

        HMODULE m_VulkanLib;
    };

}  // namespace lr::g