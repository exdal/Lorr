//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseAPI.hh"

#include "D3D12Sym.hh"

#include "D3D12SwapChain.hh"
#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    struct D3D12DescriptorBindingDesc
    {
        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
        u32 m_Count;
    };

    struct D3D12DescriptorHeap
    {
        ID3D12DescriptorHeap *m_pHandle = nullptr;
        u32 m_IncrementSize = 0;
        u32 m_CPUOffset = 0;
        u32 m_GPUOffset = 0;
    };

    struct D3D12API : BaseAPI
    {
        bool Init(BaseWindow *pWindow, u32 width, u32 height, APIFlags flags) override;

        void InitAllocators();

        /// COMMAND ///
        D3D12CommandQueue *CreateCommandQueue(CommandListType type);
        D3D12CommandAllocator *CreateCommandAllocator(CommandListType type);
        D3D12CommandList *CreateCommandList(CommandListType type);

        void BeginCommandList(CommandList *pList) override;
        void EndCommandList(CommandList *pList) override;
        void ResetCommandAllocator(CommandAllocator *pAllocator) override;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        void ExecuteCommandList(CommandList *pList, bool waitForFence) override;

        /// SYNC ///
        ID3D12Fence *CreateFence(u32 initialValue = 0);
        void DeleteFence(ID3D12Fence *pFence);
        bool IsFenceSignaled(D3D12CommandList *pList);

        /// PIPELINE ///
        ID3D12PipelineLibrary *CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);
        ID3D12RootSignature *SerializeRootSignature(PipelineLayoutSerializeDesc *pDesc, Pipeline *pPipeline);

        Shader *CreateShader(ShaderStage stage, BufferReadStream &buf) override;
        Shader *CreateShader(ShaderStage stage, eastl::string_view path) override;
        void DeleteShader(Shader *pShader) override;

        Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo) override;
        Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo) override;

        /// SWAPCHAIN ///

        // Leave `pFSD` as `nullptr` for windowed swap chain
        void CreateSwapChain(IDXGISwapChain1 *&pHandle, void *pWindowHandle, DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFSD);
        void ResizeSwapChain(u32 width, u32 height) override;
        BaseSwapChain *GetSwapChain() override;

        void BeginFrame() override;
        void EndFrame() override;

        /// RESOURCE ///
        DescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) override;
        void DeleteDescriptorSet(DescriptorSet *pSet) override;
        void UpdateDescriptorData(DescriptorSet *pSet, DescriptorSetDesc *pDesc) override;

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        void CreateDescriptorPool(const std::initializer_list<D3D12DescriptorBindingDesc> &bindings);
        D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index = -1);
        D3D12_GPU_DESCRIPTOR_HANDLE AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index = -1);

        // * Buffers * //
        ID3D12Heap *CreateHeap(u64 heapSize, bool mappable, D3D12_HEAP_FLAGS flags);

        Buffer *CreateBuffer(BufferDesc *pDesc) override;
        void DeleteBuffer(Buffer *pHandle) override;
        void CreateBufferView(Buffer *pBuffer);

        void MapMemory(Buffer *pBuffer, void *&pData) override;
        void UnmapMemory(Buffer *pBuffer) override;

        // * Images * //
        Image *CreateImage(ImageDesc *pDesc) override;
        void DeleteImage(Image *pImage) override;
        void CreateImageView(Image *pImage);

        Sampler *CreateSampler(SamplerDesc *pDesc) override;

        void SetAllocator(D3D12Buffer *pBuffer, D3D12_RESOURCE_DESC &resourceDesc, RHIAllocatorType targetAllocator);
        void SetAllocator(D3D12Image *pImage, D3D12_RESOURCE_DESC &resourceDesc, RHIAllocatorType targetAllocator);

        void BindMemory(Image *pImage);

        /// UTILITY
        void CalcOrthoProjection(XMMATRIX &mat, XMFLOAT2 viewSize, float zFar, float zNear) override;

        static i64 TFFenceWait(void *pData);

        /// ------------------------------------------------------------- ///

        // clang-format off

        /// Type conversions
        static DXGI_FORMAT                      ToDXFormat(ImageFormat format);
        static DXGI_FORMAT                      ToDXFormat(VertexAttribType format);
        static D3D_PRIMITIVE_TOPOLOGY           ToDXTopology(PrimitiveType type);
        static D3D12_CULL_MODE                  ToDXCullMode(CullMode mode);
        static D3D12_ROOT_PARAMETER_TYPE        ToDXDescriptorType(DescriptorType type);
        static D3D12_DESCRIPTOR_RANGE_TYPE      ToDXDescriptorRangeType(DescriptorType type);
        static D3D12_RESOURCE_STATES            ToDXImageUsage(ResourceUsage usage);
        static D3D12_RESOURCE_STATES            ToDXBufferUsage(ResourceUsage usage);
        static D3D12_SHADER_VISIBILITY          ToDXShaderType(ShaderStage type);
        static D3D12_COMMAND_LIST_TYPE          ToDXCommandListType(CommandListType type);

        // clang-format on

        /// ------------------------------------------------------------- ///

        /// API Handles
        ID3D12Device4 *m_pDevice = nullptr;
        IDXGIFactory4 *m_pFactory = nullptr;
        IDXGIAdapter1 *m_pAdapter = nullptr;
        D3D12SwapChain m_SwapChain;

        /// Main Queues
        D3D12CommandQueue *m_pDirectQueue = nullptr;
        D3D12CommandQueue *m_pComputeQueue = nullptr;

        /// Pools/Caches
        D3D12DescriptorHeap m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
        ID3D12PipelineLibrary *m_pPipelineCache = nullptr;

        BufferedAllocator<Memory::LinearAllocatorView, ID3D12Heap *> m_MADescriptor;
        BufferedAllocator<Memory::LinearAllocatorView, ID3D12Heap *> m_MABufferLinear;
        BufferedAllocator<Memory::TLSFAllocatorView, ID3D12Heap *> m_MABufferTLSF;
        BufferedAllocator<Memory::LinearAllocatorView, ID3D12Heap *> m_MABufferFrameTime;
        BufferedAllocator<Memory::TLSFAllocatorView, ID3D12Heap *> m_MAImageTLSF;

        EA::Thread::Thread m_FenceThread;

        static constexpr D3D_FEATURE_LEVEL kMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    };

}  // namespace lr::Graphics