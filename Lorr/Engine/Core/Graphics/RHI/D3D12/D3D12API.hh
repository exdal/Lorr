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
        D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
        u32 Count;
    };

    struct D3D12DescriptorHeap
    {
        ID3D12DescriptorHeap *pHandle = nullptr;
        u32 IncrementSize = 0;
        u32 CPUOffset = 0;
        u32 GPUOffset = 0;
    };

    struct D3D12API : BaseAPI
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

        /// SYNC ///
        ID3D12Fence *CreateFence(u32 initialValue = 0);
        void DeleteFence(ID3D12Fence *pFence);
        bool IsFenceSignaled(D3D12CommandList *pList);

        /// PIPELINE ///
        ID3D12PipelineLibrary *CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        BaseShader *CreateShader(ShaderStage stage, BufferReadStream &buf) override;
        BaseShader *CreateShader(ShaderStage stage, eastl::string_view path) override;
        void DeleteShader(BaseShader *pShader) override;

        GraphicsPipelineBuildInfo *BeginPipelineBuildInfo() override;
        BasePipeline *EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo) override;

        /// SWAPCHAIN ///

        // Leave `pFSD` as `nullptr` for windowed swap chain
        void CreateSwapChain(IDXGISwapChain1 *&pHandle, void *pWindowHandle, DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFSD);
        void ResizeSwapChain(u32 width, u32 height) override;
        BaseSwapChain *GetSwapChain() override;

        void Frame() override;

        /// RESOURCE ///
        BaseDescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) override;
        void UpdateDescriptorData(BaseDescriptorSet *pSet) override;

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        void CreateDescriptorPool(const std::initializer_list<D3D12DescriptorBindingDesc> &bindings);
        D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index = -1);
        D3D12_GPU_DESCRIPTOR_HANDLE AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index = -1);

        // * Buffers * //
        ID3D12Heap *CreateHeap(u64 heapSize, bool mappable, D3D12_HEAP_FLAGS flags);

        BaseBuffer *CreateBuffer(BufferDesc *pDesc, BufferData *pData) override;
        void DeleteBuffer(BaseBuffer *pHandle) override;
        void CreateBufferView(BaseBuffer *pBuffer);

        void MapMemory(BaseBuffer *pBuffer, void *&pData) override;
        void UnmapMemory(BaseBuffer *pBuffer) override;

        BaseBuffer *ChangeAllocator(BaseCommandList *pList, BaseBuffer *pTarget, AllocatorType targetAllocator) override;

        // * Images * //
        BaseImage *CreateImage(ImageDesc *pDesc, ImageData *pData) override;
        void DeleteImage(BaseImage *pImage) override;
        void CreateImageView(BaseImage *pImage);

        BaseSampler *CreateSampler(SamplerDesc *pDesc) override;

        void BindMemory(BaseImage *pImage);

        static i64 TFFenceWait(void *pData);

        /// ------------------------------------------------------------- ///

        // clang-format off

        /// Type conversions
        static DXGI_FORMAT                      ToDXFormat(ResourceFormat format);
        static DXGI_FORMAT                      ToDXFormat(VertexAttribType format);
        static D3D_PRIMITIVE_TOPOLOGY           ToDXTopology(PrimitiveType type);
        static D3D12_CULL_MODE                  ToDXCullMode(CullMode mode);
        static D3D12_ROOT_PARAMETER_TYPE        ToDXDescriptorType(DescriptorType type);
        
        //~ IMPORTANT: `DescriptorType` does not represent correct heap types, this is special to D3D12.
        //~ Since it's an internal thing to manage descriptor pools, we leave it as is.
        //~ For correct types, see function implementation.
        static D3D12_DESCRIPTOR_HEAP_TYPE       ToDXHeapType(DescriptorType type);

        static D3D12_DESCRIPTOR_RANGE_TYPE      ToDXDescriptorRangeType(DescriptorType type);
        static D3D12_RESOURCE_STATES            ToDXImageUsage(ResourceUsage usage);
        static D3D12_RESOURCE_STATES            ToDXBufferUsage(ResourceUsage usage);
        static D3D12_RESOURCE_STATES            ToDXImageLayout(ResourceUsage usage);
        static D3D12_SHADER_VISIBILITY          ToDXShaderType(ShaderStage type);

        // clang-format on

        /// ------------------------------------------------------------- ///

        /// API Handles
        ID3D12Device4 *m_pDevice = nullptr;
        IDXGIFactory4 *m_pFactory = nullptr;
        IDXGIAdapter1 *m_pAdapter = nullptr;
        D3D12SwapChain m_SwapChain;

        /// Main Queues
        D3D12CommandQueue *m_pDirectQueue = nullptr;

        /// Pools/Caches
        D3D12DescriptorHeap m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
        ID3D12PipelineLibrary *m_pPipelineCache = nullptr;

        BufferedAllocator<Memory::LinearAllocatorView, ID3D12Heap *> m_MADescriptor;
        BufferedAllocator<Memory::LinearAllocatorView, ID3D12Heap *> m_MABufferLinear;
        BufferedAllocator<Memory::TLSFAllocatorView, ID3D12Heap *> m_MABufferTLSF;
        BufferedAllocator<Memory::TLSFAllocatorView, ID3D12Heap *> m_MAImageTLSF;

        EA::Thread::Thread m_FenceThread;

        static constexpr D3D_FEATURE_LEVEL kMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    };

}  // namespace lr::Graphics