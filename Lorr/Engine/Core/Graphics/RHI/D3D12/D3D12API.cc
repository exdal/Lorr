#include "D3D12API.hh"

#include "D3D12Pipeline.hh"
#include "D3D12Resource.hh"

#define HRCall(func, message)                                                                                      \
    if (FAILED(hr = func))                                                                                         \
    {                                                                                                              \
        char *pError;                                                                                              \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                      NULL,                                                                                        \
                      hr,                                                                                          \
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
                      (LPTSTR)&pError,                                                                             \
                      0,                                                                                           \
                      NULL);                                                                                       \
        LOG_ERROR(message " Details: {}", pError);                                                                 \
        return;                                                                                                    \
    }

#define HRCallRet(func, message, ret)                                                                              \
    if (FAILED(hr = func))                                                                                         \
    {                                                                                                              \
        char *pError;                                                                                              \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                      NULL,                                                                                        \
                      hr,                                                                                          \
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
                      (LPTSTR)&pError,                                                                             \
                      0,                                                                                           \
                      NULL);                                                                                       \
        LOG_ERROR(message " Details: {}", pError);                                                                 \
        return ret;                                                                                                \
    }
//

// Example: `BaseCommandQueue *pQueue` -> `D3D12CommandQueue *pQueueDX`
#define API_VAR(type, name) type *name##DX = (type *)name

namespace lr::Graphics
{
    bool D3D12API::Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags)
    {
        ZoneScoped;

        HRESULT hr;

        u32 deviceFlags = 0;

#if _DEBUG
        deviceFlags |= DXGI_CREATE_FACTORY_DEBUG;

        ID3D12Debug1 *pDebug = nullptr;
        D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));

        pDebug->EnableDebugLayer();
        pDebug->SetEnableGPUBasedValidation(true);

        SAFE_RELEASE(pDebug);
#endif

        // Create factory
        HRCallRet(CreateDXGIFactory2((deviceFlags), IID_PPV_ARGS(&m_pFactory)), "Failed to create DXGI2 Factory!", false);

#pragma region Select Factory
        // Select suitable adapter
        IDXGIFactory6 *pFactory6 = nullptr;
        m_pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6));

        if (pFactory6)
        {
            for (u32 i = 0;
                 pFactory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_pAdapter)) != DXGI_ERROR_NOT_FOUND;
                 i++)
            {
                DXGI_ADAPTER_DESC1 adapterDesc = {};
                m_pAdapter->GetDesc1(&adapterDesc);

                if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;  // We don't want software adapter

                if (SUCCEEDED(D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
                {
                    LOG_INFO("Found high performance adapter! ADAPTER{}-{}", i, adapterDesc.VendorId);

                    break;
                }

                SAFE_RELEASE(m_pAdapter);
            }
        }

        if (m_pAdapter == nullptr)
        {
            for (u32 i = 0; m_pFactory->EnumAdapters1(i, &m_pAdapter) != DXGI_ERROR_NOT_FOUND; i++)
            {
                DXGI_ADAPTER_DESC1 adapterDesc = {};
                m_pAdapter->GetDesc1(&adapterDesc);

                if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;  // We don't want software adapter

                if (SUCCEEDED(D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
                {
                    LOG_INFO("Found adapter! ADAPTER{}-{}", i, adapterDesc.VendorId);

                    break;
                }

                SAFE_RELEASE(m_pAdapter);
            }
        }

        if (m_pAdapter == nullptr)
        {
            LOG_CRITICAL("GPU is not suitable for D3D12.");

            return false;
        }

#pragma endregion

        HRCallRet(D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, IID_PPV_ARGS(&m_pDevice)), "Failed to create D3D12 Device!", false);

#ifdef _DEBUG
        ID3D12InfoQueue *pInfoQueue = nullptr;
        m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));

        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        SAFE_RELEASE(pInfoQueue);
#endif

        InitAllocators();

        m_CommandListPool.Init();
        CreateDescriptorPool({
            { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256 },
            { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256 },
        });

        m_pPipelineCache = CreatePipelineCache();

        m_pDirectQueue = (D3D12CommandQueue *)CreateCommandQueue(CommandListType::Direct);
        m_SwapChain.Init(pWindow, this, SwapChainFlags::TripleBuffering);
        m_SwapChain.CreateBackBuffers(this);

        LOG_TRACE("Successfully initialized D3D12 context.");

        InitCommandLists();

        m_FenceThread.Begin(TFFenceWait, this);

        LOG_TRACE("Initialized D3D12 render context.");

        return true;
    }

    void D3D12API::InitAllocators()
    {
        ZoneScoped;

        constexpr u32 kTypeMem = Memory::MiBToBytes(8);

        m_TypeAllocator.Init(kTypeMem);
        m_pTypeData = new u8[kTypeMem];
    }

    BaseCommandQueue *D3D12API::CreateCommandQueue(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandQueue *pQueue = AllocType<D3D12CommandQueue>();

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.NodeMask = 0;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;  // TODO: Multiple Queues
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pQueue->m_pHandle));

        return pQueue;
    }

    BaseCommandAllocator *D3D12API::CreateCommandAllocator(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandAllocator *pAllocator = AllocType<D3D12CommandAllocator>();

        m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pAllocator->pHandle));

        return pAllocator;
    }

    BaseCommandList *D3D12API::CreateCommandList(CommandListType type)
    {
        ZoneScoped;

        BaseCommandAllocator *pAllocator = CreateCommandAllocator(type);
        API_VAR(D3D12CommandAllocator, pAllocator);

        D3D12CommandList *pList = AllocType<D3D12CommandList>();

        pList->m_pFence = CreateFence();
        pList->m_FenceEvent = CreateEventExA(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        ID3D12GraphicsCommandList4 *pHandle = nullptr;
        m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocatorDX->pHandle, nullptr, IID_PPV_ARGS(&pHandle));

        pList->Init(pHandle, pAllocatorDX, type);

        return pList;
    }

    void D3D12API::BeginCommandList(BaseCommandList *pList)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        pListDX->m_pAllocator->Reset();
        pListDX->Reset(pListDX->m_pAllocator);
    }

    void D3D12API::EndCommandList(BaseCommandList *pList)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        pListDX->m_pHandle->Close();
    }

    void D3D12API::ResetCommandAllocator(BaseCommandAllocator *pAllocator)
    {
        ZoneScoped;

        API_VAR(D3D12CommandAllocator, pAllocator);

        pAllocatorDX->Reset();
    }

    void D3D12API::ExecuteCommandList(BaseCommandList *pList, bool waitForFence)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        m_pDirectQueue->ExecuteCommandList(pListDX);

        if (waitForFence)
        {
            pListDX->m_pFence->SetEventOnCompletion(pListDX->m_DesiredFenceValue.load(eastl::memory_order_acquire), pListDX->m_FenceEvent);
            WaitForSingleObjectEx(pListDX->m_FenceEvent, INFINITE, false);

            m_CommandListPool.Release(pList);
        }
        else
        {
            m_CommandListPool.SignalFence(pList);
        }
    }

    ID3D12Fence *D3D12API::CreateFence(u32 initialValue)
    {
        ZoneScoped;

        ID3D12Fence *pFence = nullptr;

        m_pDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));

        return pFence;
    }

    void D3D12API::DeleteFence(ID3D12Fence *pFence)
    {
        ZoneScoped;

        SAFE_RELEASE(pFence);
    }

    bool D3D12API::IsFenceSignaled(D3D12CommandList *pList)
    {
        ZoneScoped;

        u64 completed = pList->m_pFence->GetCompletedValue();
        u64 desired = pList->m_DesiredFenceValue.load(eastl::memory_order_acquire);

        return completed == desired;
    }

    BaseRenderPass *D3D12API::CreateRenderPass(RenderPassDesc *pDesc)
    {
        ZoneScoped;

        return nullptr;
    }

    void D3D12API::DeleteRenderPass(BaseRenderPass *pRenderPass)
    {
        ZoneScoped;
    }

    ID3D12PipelineLibrary *D3D12API::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
    {
        ZoneScoped;

        ID3D12PipelineLibrary *pLibrary = nullptr;

        m_pDevice->CreatePipelineLibrary(pInitialData, initialDataSize, IID_PPV_ARGS(&pLibrary));

        return pLibrary;
    }

    void D3D12API::BeginPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        pBuildInfo = new D3D12GraphicsPipelineBuildInfo;
        pBuildInfo->Init();
    }

    BasePipeline *D3D12API::EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo, BaseRenderPass *pRenderPass)
    {
        ZoneScoped;

        API_VAR(D3D12GraphicsPipelineBuildInfo, pBuildInfo);
        D3D12Pipeline *pPipeline = AllocType<D3D12Pipeline>();

        D3D12_ROOT_PARAMETER1 pParams[8] = {};
        D3D12_DESCRIPTOR_RANGE1 ppRanges[8][8] = {};

        for (u32 i = 0; i < pBuildInfo->m_DescriptorSetCount; i++)
        {
            BaseDescriptorSet *pSet = pBuildInfo->m_ppDescriptorSets[i];

            pParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            pParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // TODO

            pParams[i].DescriptorTable.NumDescriptorRanges = pSet->BindingCount;
            pParams[i].DescriptorTable.pDescriptorRanges = ppRanges[i];

            for (u32 j = 0; j < pSet->BindingCount; j++)
            {
                D3D12_DESCRIPTOR_RANGE1 &range = ppRanges[i][j];

                range.NumDescriptors = 1;
                range.RangeType = ToDXDescriptorRangeType(pSet->pBindingInfos[j].Type);
                range.BaseShaderRegister = 0;
                range.RegisterSpace = 0;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.NumParameters = pBuildInfo->m_DescriptorSetCount;
        rootSignatureDesc.Desc_1_1.pParameters = pParams;

        ID3DBlob *pRootSigBlob = nullptr;
        ID3DBlob *pErrorBlob = nullptr;
        D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &pRootSigBlob, &pErrorBlob);

        m_pDevice->CreateRootSignature(0, pRootSigBlob, pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pPipeline->pLayout));
        pBuildInfoDX->m_CreateInfo.pRootSignature = pPipeline->pLayout;

        // TODO: m_CreateInfo->RTVFormats

        m_pDevice->CreateGraphicsPipelineState(&pBuildInfoDX->m_CreateInfo, IID_PPV_ARGS(&pPipeline->pHandle));

        delete pBuildInfoDX;

        return pPipeline;
    }

    void D3D12API::CreateSwapChain(IDXGISwapChain1 *&pHandle, void *pWindowHandle, DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFSD)
    {
        ZoneScoped;

        m_pFactory->CreateSwapChainForHwnd(m_pDirectQueue->m_pHandle, (HWND)pWindowHandle, &desc, pFSD, nullptr, &pHandle);
    }

    void D3D12API::ResizeSwapChain(u32 width, u32 height)
    {
        ZoneScoped;
    }

    void D3D12API::Frame()
    {
        ZoneScoped;

        BaseCommandList *pList = GetCommandList();
        API_VAR(D3D12CommandList, pList);

        ID3D12GraphicsCommandList4 *pGList = pListDX->m_pHandle;

        BeginCommandList(pList);

        D3D12Image &image = m_SwapChain.m_pFrames[m_SwapChain.m_CurrentFrame].Image;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = image.m_pHandle;

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        pGList->ResourceBarrier(1, &barrier);

        static XMFLOAT4 clear = { 0.0, 0, 0, 0 };
        pGList->ClearRenderTargetView(image.m_ViewHandle, &clear.x, 0, nullptr);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        pGList->ResourceBarrier(1, &barrier);

        EndCommandList(pList);
        ExecuteCommandList(pList, false);

        m_SwapChain.Present();
        m_SwapChain.NextFrame();
    }

    BaseDescriptorSet *D3D12API::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        D3D12DescriptorSet *pDescriptorSet = AllocType<D3D12DescriptorSet>();

        D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc = {};

        D3D12_ROOT_PARAMETER1 pBindings[8] = {};

        for (u32 i = 0; i < pDesc->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->pBindings[i];

            pBindings[i].ParameterType = ToDXDescriptorType(element.Type);
            pBindings[i].ShaderVisibility = ToDXShaderType(element.TargetShader);

            // Constant Buffers
            pBindings[i].Constants.ShaderRegister = i;
            pBindings[i].Constants.Num32BitValues = element.ArraySize;

            // Uniform Buffers
            // TODO
        }

        // // First we need layout info
        // VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        // layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        // layoutCreateInfo.bindingCount = pDesc->BindingCount;
        // layoutCreateInfo.pBindings = pBindings;

        // vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pDescriptorSet->pSetLayoutHandle);

        // // And the actual set
        // VkDescriptorSetAllocateInfo allocateCreateInfo = {};
        // allocateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        // allocateCreateInfo.descriptorPool = m_pDescriptorPool;
        // allocateCreateInfo.descriptorSetCount = 1;
        // allocateCreateInfo.pSetLayouts = &pDescriptorSet->pSetLayoutHandle;

        // vkAllocateDescriptorSets(m_pDevice, &allocateCreateInfo, &pDescriptorSet->pHandle);

        /// ---------------------------------------------------------- ///

        pDescriptorSet->BindingCount = pDesc->BindingCount;
        memcpy(pDescriptorSet->pBindingInfos, pDesc->pBindings, pDesc->BindingCount * sizeof(DescriptorBindingDesc));

        return pDescriptorSet;
    }

    void D3D12API::UpdateDescriptorData(BaseDescriptorSet *pSet, DescriptorSetDesc *pDesc)
    {
        ZoneScoped;
    }

    void D3D12API::CreateDescriptorPool(const std::initializer_list<D3D12DescriptorBindingDesc> &bindings)
    {
        ZoneScoped;

        for (auto &element : bindings)
        {
            D3D12DescriptorHeap &heap = m_pDescriptorHeaps[element.HeapType];
            ID3D12DescriptorHeap *pHandle = heap.pHandle;

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = element.HeapType;
            heapDesc.NumDescriptors = element.Count;
            // heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHandle));

            heap.pHandle = pHandle;
            heap.IncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(element.HeapType);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12API::AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ZoneScoped;

        D3D12DescriptorHeap &heap = m_pDescriptorHeaps[type];

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.pHandle->GetCPUDescriptorHandleForHeapStart();

        if (index == -1)
        {
            handle.ptr += heap.IncrementSize * heap.Offset;
            heap.Offset++;
        }
        else
        {
            handle.ptr += heap.IncrementSize * index;
        }

        return handle;
    }

    BaseImage *D3D12API::CreateImage(ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        D3D12Image *pImage = AllocType<D3D12Image>();

        return pImage;
    }

    void D3D12API::DeleteImage(BaseImage *pImage)
    {
        ZoneScoped;
    }

    void D3D12API::CreateRenderTarget(BaseImage *pImage)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        pImageDX->m_ViewHandle = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
        viewDesc.Format = ToDXFormat(pImage->m_Format);
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipSlice = 0;

        m_pDevice->CreateRenderTargetView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_ViewHandle);
    }

    i64 D3D12API::TFFenceWait(void *pData)
    {
        D3D12API *pAPI = (D3D12API *)pData;
        CommandListPool &commandPool = pAPI->m_CommandListPool;
        auto &directFenceMask = commandPool.m_DirectFenceMask;

        while (true)
        {
            u32 mask = directFenceMask.load(eastl::memory_order_acquire);

            // Iterate over all signaled fences
            while (mask != 0)
            {
                u32 index = Memory::GetLSB(mask);

                BaseCommandList *pList = commandPool.m_DirectLists[index];
                API_VAR(D3D12CommandList, pList);

                if (pAPI->IsFenceSignaled(pListDX))
                {
                    commandPool.Release(pList);
                    commandPool.ReleaseFence(index);
                }

                mask = directFenceMask.load(eastl::memory_order_acquire);
            }
        }

        return 1;
    }

    /// ---------------- Type Conversion LUTs ---------------- ///

    constexpr DXGI_FORMAT kFormatLUT[] = {
        DXGI_FORMAT_UNKNOWN,              // Unknown
        DXGI_FORMAT_BC1_UNORM,            // BC1
        DXGI_FORMAT_R8G8B8A8_UNORM,       // RGBA8F
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,  // RGBA8_SRGBF
        DXGI_FORMAT_B8G8R8A8_UNORM,       // BGRA8F
        DXGI_FORMAT_R16G16B16A16_FLOAT,   // RGBA16F
        DXGI_FORMAT_R32G32B32A32_FLOAT,   // RGBA32F
        DXGI_FORMAT_R32_UINT,             // R32U
        DXGI_FORMAT_R32_FLOAT,            // R32F
        DXGI_FORMAT_D32_FLOAT,            // D32F
        DXGI_FORMAT_D24_UNORM_S8_UINT,    // D32FS8U
    };

    constexpr DXGI_FORMAT kAttribFormatLUT[] = {
        DXGI_FORMAT_UNKNOWN,
        DXGI_FORMAT_R32_FLOAT,           // Float
        DXGI_FORMAT_R32G32_FLOAT,        // Vec2
        DXGI_FORMAT_R32G32B32_FLOAT,     // Vec3
        DXGI_FORMAT_R11G11B10_FLOAT,     // Vec3_Packed
        DXGI_FORMAT_R32G32B32A32_FLOAT,  // Vec4
        DXGI_FORMAT_R32_UINT,            // UInt
        DXGI_FORMAT_R8G8B8A8_UNORM,      // Vec4U_Packed
    };

    constexpr D3D_PRIMITIVE_TOPOLOGY kPrimitiveLUT[] = {
        D3D_PRIMITIVE_TOPOLOGY_POINTLIST,                  // PointList
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,                   // LineList
        D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,                  // LineStrip
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,               // TriangleList
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,              // TriangleStrip
        D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,  // Patch
    };

    constexpr D3D12_CULL_MODE kCullModeLUT[] = {
        D3D12_CULL_MODE_NONE,   // None
        D3D12_CULL_MODE_FRONT,  // Front
        D3D12_CULL_MODE_BACK,   // Back
    };

    constexpr D3D12_ROOT_PARAMETER_TYPE kDescriptorTypeLUT[] = {
        D3D12_ROOT_PARAMETER_TYPE_SRV,  // ShaderResourceView
        D3D12_ROOT_PARAMETER_TYPE_CBV,  // ConstantBufferView
        D3D12_ROOT_PARAMETER_TYPE_UAV,  // UnorderedAccessBuffer
        D3D12_ROOT_PARAMETER_TYPE_UAV,  // UnorderedAccessView
    };

    constexpr D3D12_DESCRIPTOR_HEAP_TYPE kDescriptorHeapTypeLUT[] = {
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,      // Sampler
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // ShaderResourceView
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // ConstantBufferView
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // UnorderedAccessBuffer
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // UnorderedAccessView
        D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES,    // RootConstant
    };

    constexpr D3D12_DESCRIPTOR_RANGE_TYPE kDescriptorRangeLUT[] = {
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,  // ShaderResourceView
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV,  // ConstantBufferView
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,  // UnorderedAccessBuffer
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,  // UnorderedAccessView
    };

    constexpr D3D12_SHADER_VISIBILITY kShaderTypeLUT[] = {
        D3D12_SHADER_VISIBILITY_VERTEX,    // Vertex
        D3D12_SHADER_VISIBILITY_PIXEL,     // Pixel
        D3D12_SHADER_VISIBILITY_ALL,       // Compute
        D3D12_SHADER_VISIBILITY_HULL,      // Hull
        D3D12_SHADER_VISIBILITY_DOMAIN,    // Domain
        D3D12_SHADER_VISIBILITY_GEOMETRY,  // Geometry
    };

    DXGI_FORMAT D3D12API::ToDXFormat(ResourceFormat format)
    {
        return kFormatLUT[(u32)format];
    }

    DXGI_FORMAT D3D12API::ToDXFormat(VertexAttribType format)
    {
        return kAttribFormatLUT[(u32)format];
    }

    D3D_PRIMITIVE_TOPOLOGY D3D12API::ToDXTopology(PrimitiveType type)
    {
        return kPrimitiveLUT[(u32)type];
    }

    D3D12_CULL_MODE D3D12API::ToDXCullMode(CullMode mode)
    {
        return kCullModeLUT[(u32)mode];
    }

    D3D12_ROOT_PARAMETER_TYPE D3D12API::ToDXDescriptorType(DescriptorType type)
    {
        return kDescriptorTypeLUT[(u32)type];
    }

    D3D12_DESCRIPTOR_HEAP_TYPE D3D12API::ToDXHeapType(DescriptorType type)
    {
        return kDescriptorHeapTypeLUT[(u32)type];
    }

    D3D12_DESCRIPTOR_RANGE_TYPE D3D12API::ToDXDescriptorRangeType(DescriptorType type)
    {
        return kDescriptorRangeLUT[(u32)type];
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXImageUsage(ImageUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage & ImageUsage::ColorAttachment)
            v |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (usage & ImageUsage::DepthAttachment)
            v |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if (usage & ImageUsage::Sampled)
            v |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (usage & ImageUsage::CopySrc)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if (usage & ImageUsage::CopyDst)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        return v;
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXBufferUsage(BufferUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage & BufferUsage::Vertex)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if (usage & BufferUsage::Index)
            v |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if (usage & BufferUsage::ConstantBuffer)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if (usage & BufferUsage::Unordered)
            v |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        if (usage & BufferUsage::Indirect)
            v |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        if (usage & BufferUsage::CopySrc)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if (usage & BufferUsage::CopyDst)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        return v;
    }

    D3D12_SHADER_VISIBILITY D3D12API::ToDXShaderType(ShaderType type)
    {
        return kShaderTypeLUT[(u32)type];
    }

}  // namespace lr::Graphics