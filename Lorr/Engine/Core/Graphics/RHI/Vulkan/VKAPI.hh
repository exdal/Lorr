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
        VkDescriptorType m_Type;
        u32 m_Count;
    };

    /// ------------------------------------------ ///

    struct VKAPI : BaseAPI
    {
        bool Init(BaseAPIDesc *pDesc) override;
        void InitAllocators(APIAllocatorInitDesc *pDesc) override;
        
        /// COMMAND ///
        VKCommandQueue *CreateCommandQueue(CommandListType type);
        VKCommandAllocator *CreateCommandAllocator(CommandListType type);
        VKCommandList *CreateCommandList(CommandListType type);

        void BeginCommandList(CommandList *pList) override;
        void EndCommandList(CommandList *pList) override;
        void ResetCommandAllocator(CommandAllocator *pAllocator) override;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        void ExecuteCommandList(CommandList *pList, bool waitForFence) override;
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
        VkPipelineLayout SerializePipelineLayout(PipelineLayoutSerializeDesc *pDesc, Pipeline *pPipeline);

        Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo) override;
        Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo) override;

        /// SWAPCHAIN ///
        void CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info);
        void DeleteSwapChain(VKSwapChain *pSwapChain);
        void GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages);
        void ResizeSwapChain(u32 width, u32 height) override;
        BaseSwapChain *GetSwapChain() override;

        void BeginFrame() override;
        void EndFrame() override;
        void WaitForDevice();

        /// RESOURCE ///
        // * Shaders * //
        Shader *CreateShader(ShaderStage stage, BufferReadStream &buf) override;
        Shader *CreateShader(ShaderStage stage, eastl::string_view path) override;
        void DeleteShader(Shader *pShader) override;

        DescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) override;
        void DeleteDescriptorSet(DescriptorSet *pSet) override;
        void UpdateDescriptorData(DescriptorSet *pSet, DescriptorSetDesc *pDesc) override;

        VkDescriptorPool CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts);

        // * Buffers * //
        VkDeviceMemory CreateHeap(u64 heapSize, bool cpuWrite);

        Buffer *CreateBuffer(BufferDesc *pDesc) override;
        void DeleteBuffer(Buffer *pHandle) override;

        void MapMemory(Buffer *pBuffer, void *&pData) override;
        void UnmapMemory(Buffer *pBuffer) override;

        // * Images * //
        Image *CreateImage(ImageDesc *pDesc) override;
        void DeleteImage(Image *pImage) override;
        void CreateImageView(Image *pImage);

        Sampler *CreateSampler(SamplerDesc *pDesc) override;
        void DeleteSampler(VkSampler pSampler);

        void SetAllocator(VKBuffer *pBuffer, APIAllocatorType targetAllocator);
        void SetAllocator(VKImage *pImage, APIAllocatorType targetAllocator);

        /// UTILITY
        ImageFormat &GetSwapChainImageFormat() override;
        void CalcOrthoProjection(Camera2D &camera) override;
        void CalcPerspectiveProjection(Camera3D &camera) override;

        // * Device Features * //
        bool IsFormatSupported(ImageFormat format, VkColorSpaceKHR *pColorSpaceOut);
        bool IsPresentModeSupported(VkPresentModeKHR format);
        void GetSurfaceCapabilities(VkSurfaceCapabilitiesKHR &capabilitiesOut);
        u32 GetMemoryTypeIndex(u32 setBits, VkMemoryPropertyFlags propFlags);
        u32 GetMemoryTypeIndex(VkMemoryPropertyFlags propFlags);

        // * API Instance * //
        bool LoadVulkan();
        bool SetupInstance(BaseWindow *pWindow);
        bool SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);
        bool SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);

        static i64 TFFenceWait(void *pData);

        /// ------------------------------------------------------------- ///

        // clang-format off

        /// Type conversions
        static VkFormat                 ToVKFormat(ImageFormat format);
        static VkFormat                 ToVKFormat(VertexAttribType format);
        static VkPrimitiveTopology      ToVKTopology(PrimitiveType type);
        static VkCullModeFlags          ToVKCullMode(CullMode mode);
        static VkDescriptorType         ToVKDescriptorType(DescriptorType type);
        static VkImageUsageFlags        ToVKImageUsage(ResourceUsage usage);
        static VkBufferUsageFlagBits    ToVKBufferUsage(ResourceUsage usage);
        static VkImageLayout            ToVKImageLayout(ResourceUsage usage);
        static VkShaderStageFlagBits    ToVKShaderType(ShaderStage type);
        static VkPipelineStageFlags2    ToVKPipelineStage(PipelineStage stage);
        static VkAccessFlags2           ToVKAccessFlags(PipelineAccess access);

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
        VKCommandQueue *m_pComputeQueue = nullptr;
        VKCommandQueue *m_pTransferQueue = nullptr;

        /// Pools/Caches
        VkPipelineCache m_pPipelineCache = nullptr;
        VkDescriptorPool m_pDescriptorPool = nullptr;

        BufferedAllocator<Memory::LinearAllocatorView, VkDeviceMemory> m_MADescriptor;
        BufferedAllocator<Memory::LinearAllocatorView, VkDeviceMemory> m_MABufferLinear;
        BufferedAllocator<Memory::TLSFAllocatorView, VkDeviceMemory> m_MABufferTLSF;
        BufferedAllocator<Memory::TLSFAllocatorView, VkDeviceMemory> m_MABufferTLSFHost;
        BufferedAllocator<Memory::LinearAllocatorView, VkDeviceMemory> m_MABufferFrametime;
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