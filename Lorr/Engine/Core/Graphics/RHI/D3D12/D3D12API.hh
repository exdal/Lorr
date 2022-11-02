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
        u32 Offset = 0;
    };

    struct D3D12API : BaseAPI
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

        /// SYNC ///
        ID3D12Fence *CreateFence(u32 initialValue = 0);
        void DeleteFence(ID3D12Fence *pFence);
        bool IsFenceSignaled(D3D12CommandList *pList);

        /// PIPELINE ///
        ID3D12PipelineLibrary *CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);

        void BeginPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo);
        BasePipeline *EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo);

        /// SWAPCHAIN ///

        // Leave `pFSD` as `nullptr` for windowed swap chain
        void CreateSwapChain(IDXGISwapChain1 *&pHandle, void *pWindowHandle, DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFSD);
        void ResizeSwapChain(u32 width, u32 height);

        void Frame();

        /// RESOURCE ///
        BaseDescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc);
        void UpdateDescriptorData(BaseDescriptorSet *pSet, DescriptorSetDesc *pDesc);

        // `VKDescriptorBindingDesc::Type` represents descriptor type.
        // `VKDescriptorBindingDesc::ArraySize` represents descriptor count for that type.
        // Same types cannot be used, will result in UB. Other variables are ignored.
        void CreateDescriptorPool(const std::initializer_list<D3D12DescriptorBindingDesc> &bindings);
        D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index = -1);

        // * Images * //
        BaseImage *CreateImage(ImageDesc *pDesc, ImageData *pData);
        void DeleteImage(BaseImage *pImage);

        void CreateRenderTarget(BaseImage *pImage);

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
        static D3D12_RESOURCE_STATES            ToDXImageUsage(ImageUsage usage);
        static D3D12_RESOURCE_STATES            ToDXBufferUsage(BufferUsage usage);
        static D3D12_SHADER_VISIBILITY          ToDXShaderType(ShaderType type);

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

        EA::Thread::Thread m_FenceThread;

        static constexpr D3D_FEATURE_LEVEL kMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    };

}  // namespace lr::Graphics