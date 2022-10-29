//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseAPI.hh"

#include "VKSym.hh"

#include "VKCommandQueue.hh"
#include "VKPipeline.hh"
#include "VKSwapChain.hh"

namespace lr::Graphics
{
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

    // TODO: Temp state manager, move this to actual renderer stuff once its done
    struct APIStateManager
    {
        void Init(VKAPI *pAPI);

        void UpdateCamera3DData(XMMATRIX mat);
        void UpdateCamera2DData(XMMATRIX mat);

        /// PIPELINES ///
        VKPipeline m_GeometryPipeline;
        VKPipeline m_UIPipeline;

        VkFramebuffer m_pGeometryRTV = nullptr;
        VkFramebuffer m_pUIRTV = nullptr;

        /// RENDERPASS ///
        VkRenderPass m_pGeometryPass = nullptr;
        VkRenderPass m_pUIPass = nullptr;
        VkRenderPass m_pPresentPass = nullptr;

        /// DESCRIPTORS ///
        // * MAT4 descriptor just for camera matrix
        VKDescriptorSet m_Camera3DDescriptor;
        VKBuffer m_Camera3DBuffer;
        VKDescriptorSet m_Camera2DDescriptor;
        VKBuffer m_Camera2DBuffer;

        VKDescriptorSet m_UISamplerDescriptor;

        VKAPI *m_pAPI = nullptr;
    };

    struct VKAPI : BaseAPI
    {
        bool Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags);

        void InitAllocators();

        /// COMMAND ///
        BaseCommandQueue *CreateCommandQueue(CommandListType type);
        BaseCommandAllocator *CreateCommandAllocator(CommandListType type);
        BaseCommandList *CreateCommandList(CommandListType type);

        void BeginCommandList(BaseCommandList *pList);
        void EndCommandList(BaseCommandList *pList);
        void ResetCommandAllocator(BaseCommandAllocator *pAllocator);

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        void ExecuteCommandList(BaseCommandList *pList, bool waitForFence);
        // Executes command list of current frame, it always performs a present operation.
        void PresentCommandQueue();

        /// SYNC ///
        VkFence CreateFence(bool signaled);
        void DeleteFence(VkFence pFence);
        bool IsFenceSignaled(VkFence pFence);
        void ResetFence(VkFence pFence);

        VkSemaphore CreateSemaphore(u32 initialValue = 0, bool binary = true);
        void DeleteSemaphore(VkSemaphore pSemaphore);

        /// RENDERPASS ///
        BaseRenderPass *CreateRenderPass(RenderPassDesc *pDesc);
        void DeleteRenderPass(BaseRenderPass *pRenderPass);

        /// PIPELINE ///
        VkPipelineCache CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        void BeginPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo);
        BasePipeline *EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo, BaseRenderPass *pRenderPass);

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

        BaseDescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc);
        void UpdateDescriptorData(BaseDescriptorSet *pSet, DescriptorSetDesc *pDesc);

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        VkDescriptorPool CreateDescriptorPool(const std::initializer_list<DescriptorBindingDesc> &layouts);

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
        void CreateSampler(VKImage *pHandle);
        VkFramebuffer CreateFramebuffer(XMUINT2 size, u32 attachmentCount, VKImage *pAttachments, VkRenderPass &pRenderPass);
        void DeleteFramebuffer(VkFramebuffer pFramebuffer);

        void AllocateImageMemory(VKImage *pImage, AllocatorType allocatorType);
        void FreeImageMemory(VKImage *pImage);

        void BindMemory(VKImage *pImage);

        // * Device Features * //
        bool IsFormatSupported(ResourceFormat format, VkColorSpaceKHR *pColorSpaceOut);
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
        static VkImageUsageFlags ToVulkanImageUsage(ImageUsage usage);
        static VkBufferUsageFlagBits ToVulkanBufferUsage(BufferUsage usage);
        static VkShaderStageFlagBits ToVulkanShaderType(ShaderType type);

        /// ------------------------------------------------------------- ///

        /// API Handles
        VkInstance m_pInstance = nullptr;
        VkPhysicalDevice m_pPhysicalDevice = nullptr;
        VkSurfaceKHR m_pSurface = nullptr;
        VkDevice m_pDevice = nullptr;
        VKSwapChain m_SwapChain;

        APIStateManager m_APIStateMan;

        /// Main Queues
        u32 m_AvailableQueueCount = 0;
        eastl::array<u32, 3> m_QueueIndexes;
        VKCommandQueue *m_pDirectQueue = nullptr;

        /// Pools/Caches
        VkPipelineCache m_pPipelineCache = nullptr;
        VkDescriptorPool m_pDescriptorPool = nullptr;
        VKLinearMemoryAllocator m_MADescriptor;
        VKLinearMemoryAllocator m_MABufferLinear;
        VKTLSFMemoryAllocator m_MABufferTLSF;
        VKTLSFMemoryAllocator m_MAImageTLSF;

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