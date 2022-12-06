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
    struct VKDescriptorBindingDesc
    {
        VkDescriptorType Type;
        u32 Count;
    };

    /// ------------------------------------------ ///

    struct VKAPI : BaseAPI
    {
        bool Init(BaseWindow *pWindow, u32 width, u32 height, APIFlags flags) override;

        void InitAllocators() override;

        /// COMMAND ///
        BaseCommandQueue *CreateCommandQueue(CommandListType type) override;
        BaseCommandAllocator *CreateCommandAllocator(CommandListType type) override;
        BaseCommandList *CreateCommandList(CommandListType type) override;

        void BeginCommandList(BaseCommandList *pList) override;
        void EndCommandList(BaseCommandList *pList) override;
        void ResetCommandAllocator(BaseCommandAllocator *pAllocator) override;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        void ExecuteCommandList(BaseCommandList *pList, bool waitForFence) override;
        // Executes command list of current frame, it always performs a present operation.
        void PresentCommandQueue();

        /// SYNC ///
        VkFence CreateFence(bool signaled);
        void DeleteFence(VkFence pFence);
        bool IsFenceSignaled(VkFence pFence);
        void ResetFence(VkFence pFence);

        VkSemaphore CreateSemaphore(u32 initialValue = 0, bool binary = true);
        void DeleteSemaphore(VkSemaphore pSemaphore);

        /// PIPELINE ///
        VkPipelineCache CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        GraphicsPipelineBuildInfo *BeginPipelineBuildInfo() override;
        BasePipeline *EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo) override;

        /// SWAPCHAIN ///
        void CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info);
        void DeleteSwapChain(VKSwapChain *pSwapChain);
        void GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages);
        void ResizeSwapChain(u32 width, u32 height) override;
        BaseSwapChain *GetSwapChain() override;

        void Frame() override;
        void WaitForDevice();

        /// RESOURCE ///
        // * Shaders * //
        BaseShader *CreateShader(ShaderStage stage, BufferReadStream &buf) override;
        BaseShader *CreateShader(ShaderStage stage, eastl::string_view path) override;
        void DeleteShader(BaseShader *pShader) override;

        BaseDescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) override;
        void UpdateDescriptorData(BaseDescriptorSet *pSet) override;

        VkDescriptorPool CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts);

        // * Buffers * //
        VkDeviceMemory CreateHeap(u64 heapSize, bool cpuWrite);

        BaseBuffer *CreateBuffer(BufferDesc *pDesc, BufferData *pData) override;
        void DeleteBuffer(BaseBuffer *pHandle) override;

        void MapMemory(BaseBuffer *pBuffer, void *&pData) override;
        void UnmapMemory(BaseBuffer *pBuffer) override;

        BaseBuffer *ChangeAllocator(BaseCommandList *pList, BaseBuffer *pTarget, AllocatorType targetAllocator) override;

        // * Images * //
        BaseImage *CreateImage(ImageDesc *pDesc, ImageData *pData) override;
        void DeleteImage(BaseImage *pImage) override;
        void CreateImageView(BaseImage *pImage);

        BaseSampler *CreateSampler(SamplerDesc *pDesc) override;

        void SetAllocator(VKBuffer *pBuffer, AllocatorType targetAllocator);
        void SetAllocator(VKImage *pImage, AllocatorType targetAllocator);

        // * Device Features * //
        bool IsFormatSupported(ResourceFormat format, VkColorSpaceKHR *pColorSpaceOut);
        bool IsPresentModeSupported(VkPresentModeKHR format);
        void GetSurfaceCapabilities(VkSurfaceCapabilitiesKHR &capabilitiesOut);
        u32 GetMemoryTypeIndex(u32 setBits, VkMemoryPropertyFlags propFlags);
        u32 GetMemoryTypeIndex(VkMemoryPropertyFlags propFlags);

        // * API Instance * //
        bool LoadVulkan();
        bool SetupInstance(void *pHandle);
        bool SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);
        bool SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);

        static i64 TFFenceWait(void *pData);

        /// ------------------------------------------------------------- ///

        // clang-format off

        /// Type conversions
        static VkFormat                 ToVKFormat(ResourceFormat format);
        static VkFormat                 ToVKFormat(VertexAttribType format);
        static VkPrimitiveTopology      ToVKTopology(PrimitiveType type);
        static VkCullModeFlags          ToVKCullMode(CullMode mode);
        static VkDescriptorType         ToVKDescriptorType(DescriptorType type);
        static VkImageUsageFlags        ToVKImageUsage(ResourceUsage usage);
        static VkBufferUsageFlagBits    ToVKBufferUsage(ResourceUsage usage);
        static VkImageLayout            ToVKImageLayout(ResourceUsage usage);
        static VkShaderStageFlagBits    ToVKShaderType(ShaderStage type);
        //~ This function takes the shader stages as general, to set a specific stage
        //~ use this together with `ToVKPipelineShaderStage`
        static VkPipelineStageFlags     ToVKPipelineStage(ResourceUsage usage);
        static VkPipelineStageFlags     ToVKPipelineShaderStage(ShaderStage type);
        static VkAccessFlags            ToVKAccessFlags(ResourceUsage usage);

        // clang-format on

        /// ------------------------------------------------------------- ///

        /// API Handles
        VkInstance m_pInstance = nullptr;
        VkPhysicalDevice m_pPhysicalDevice = nullptr;
        VkSurfaceKHR m_pSurface = nullptr;
        VkDevice m_pDevice = nullptr;
        VKSwapChain m_SwapChain;

        /// Main Queues
        u32 m_AvailableQueueCount = 0;
        eastl::array<u32, 3> m_QueueIndexes;
        VKCommandQueue *m_pDirectQueue = nullptr;

        /// Pools/Caches
        VkPipelineCache m_pPipelineCache = nullptr;
        VkDescriptorPool m_pDescriptorPool = nullptr;

        BufferedAllocator<Memory::LinearAllocatorView, VkDeviceMemory> m_MADescriptor;
        BufferedAllocator<Memory::LinearAllocatorView, VkDeviceMemory> m_MABufferLinear;
        BufferedAllocator<Memory::TLSFAllocatorView, VkDeviceMemory> m_MABufferTLSF;
        BufferedAllocator<Memory::TLSFAllocatorView, VkDeviceMemory> m_MAImageTLSF;

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