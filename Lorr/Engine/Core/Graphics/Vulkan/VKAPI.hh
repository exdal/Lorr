//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include <eathread/eathread_pool.h>

#include "Core/Graphics/APIConfig.hh"

#include "Core/IO/BufferStream.hh"
#include "Core/IO/Memory.hh"
#include "Core/IO/MemoryAllocator.hh"

#include "VKSym.hh"

#include "VKCommandQueue.hh"
#include "VKPipeline.hh"
#include "VKSwapChain.hh"

namespace lr::Graphics
{
    /// -------------- RenderPasses --------------

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
        RenderPassAttachment *ppColorAttachments[APIConfig::kMaxColorAttachmentCount];
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

    struct APIStateManager
    {
        void Init(VKAPI *pAPI);

        void UpdateCamera3DData(XMMATRIX mat);
        void UpdateCamera2DData(XMMATRIX mat);

        /// PIPELINES ///
        VKPipeline m_PresentPipeline;
        VKPipeline m_UIPipeline;

        /// RENDERPASS ///
        VkRenderPass m_pPresentPass = nullptr;
        VkRenderPass m_pUIPass = nullptr;

        /// DESCRIPTORS ///
        // * MAT4 descriptor just for camera matrix
        VKDescriptorSet m_Camera3DDescriptor;
        VKBuffer m_Camera3DBuffer;
        VKDescriptorSet m_Camera2DDescriptor;
        VKBuffer m_Camera2DBuffer;

        VKAPI *m_pAPI = nullptr;
    };

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

        VKCommandList *GetCommandList();

        void BeginCommandList(VKCommandList *pList);
        void EndCommandList(VKCommandList *pList);
        void ResetCommandAllocator(VKCommandAllocator *pAllocator);

        // Note: `vkQueueSubmit` is not thread-safe.
        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        void ExecuteCommandList(VKCommandList *pList);
        // Executes command list of current frame, it always performs a present operation.
        void PresentCommandQueue();

        /// SYNC OBJECTS ///
        VkFence CreateFence(bool signaled);
        void DeleteFence(VkFence pFence);

        VkSemaphore CreateSemaphore(u32 initialValue = 0, bool binary = true);
        void DeleteSemaphore(VkSemaphore pSemaphore);

        /// RENDERPASS ///
        VkRenderPass CreateRenderPass(RenderPassDesc *pDesc);
        void DeleteRenderPass(VkRenderPass pRenderPass);

        /// PIPELINE ///
        VkPipelineCache CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        void BeginPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo, VkRenderPass pRenderPass);
        void EndPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo,
                                  VKPipeline *pTargetHandle,
                                  const std::initializer_list<VKDescriptorSet *> &sets);

        /// SWAPCHAIN ///
        void CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info);
        void DeleteSwapChain(VKSwapChain *pSwapChain);
        void GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages);
        void ResizeSwapChain(u32 width, u32 height);

        void Frame();
        void WaitForDevice();

        /// RESOURCE ///
        // * Shaders * //
        VkShaderModule CreateShaderModule(BufferReadStream &buf);
        VkShaderModule CreateShaderModule(eastl::string_view path);
        void DeleteShaderModule(VkShaderModule pShader);

        void CreateDescriptorSetLayout(VKDescriptorSet *pSet, VKDescriptorSetDesc *pDesc);

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        VkDescriptorPool CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts);

        // * Buffers * //
        void CreateBuffer(VKBuffer *pHandle, BufferDesc *pDesc, BufferData *pData);
        void DeleteBuffer(VKBuffer *pHandle);

        void AllocateBufferMemory(VKBuffer *pBuffer, AllocatorType allocatorType);
        void FreeBufferMemory(VKBuffer *pBuffer);

        void BindMemory(VKBuffer *pBuffer);
        void MapMemory(VKBuffer *pBuffer, void *&pData);
        void UnmapMemory(VKBuffer *pBuffer);

        void TransferBufferMemory(VKCommandList *pList, VKBuffer *pSrc, VKBuffer *pDst, AllocatorType dstAllocator);

        // * Images * //
        void CreateImage(VKImage *pHandle, ImageDesc *pDesc, ImageData *pData);
        void DeleteImage(VKImage *pImage);

        void CreateImageView(VKImage *pHandle);
        VkFramebuffer CreateFramebuffer(XMUINT2 size, u32 attachmentCount, VKImage *pAttachments, VkRenderPass &pRenderPass);
        void DeleteFramebuffer(VkFramebuffer pFramebuffer);

        void AllocateImageMemory(VKImage *pImage, AllocatorType allocatorType);
        void FreeImageMemory(VKImage *pImage);

        void BindMemory(VKImage *pImage);

        // * Device Features * //
        bool IsFormatSupported(VkFormat format, VkColorSpaceKHR *pColorSpaceOut);
        bool IsPresentModeSupported(VkPresentModeKHR format);
        void GetSurfaceCapabilities(VkSurfaceCapabilitiesKHR &capabilitiesOut);
        u32 GetMemoryTypeIndex(u32 setBits, VkMemoryPropertyFlags propFlags);

        // * API Instance * //
        bool LoadVulkan();
        bool SetupInstance(HWND windowHandle);
        bool SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);
        bool SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);

        static i64 TFFenceWait(void *pData);

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

        APIStateManager m_APIStateMan;

        /// Main Queues
        u32 m_AvailableQueueCount = 0;
        eastl::array<u32, 3> m_QueueIndexes;
        VKCommandQueue m_DirectQueue;

        /// Pools/Caches
        CommandListPool m_CommandListPool;
        VKPipelineManager m_PipelineManager;
        VkDescriptorPool m_pDescriptorPool = nullptr;

        VKLinearMemoryAllocator m_MADescriptor;
        VKLinearMemoryAllocator m_MABufferLinear;
        VKTLSFMemoryAllocator m_MABufferTLSF;
        VKTLSFMemoryAllocator m_MAImageTLSF;

        u32 m_TransientBufferCount = 0;
        VKBuffer *m_pTransientBuffers[APIConfig::kTransientBufferCount];

        /// Native API
        VkPhysicalDeviceMemoryProperties m_MemoryProps;

        VkSurfaceFormatKHR *m_pValidSurfaceFormats = nullptr;
        u32 m_ValidSurfaceFormatCount = 0;

        VkPresentModeKHR *m_pValidPresentModes = nullptr;
        u32 m_ValidPresentModeCount = 0;

        HMODULE m_VulkanLib;

        EA::Thread::Thread m_FenceThread;
    };

}  // namespace lr::Graphics