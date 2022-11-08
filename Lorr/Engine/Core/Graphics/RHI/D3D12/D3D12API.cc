#include "D3D12API.hh"

#include "D3D12Pipeline.hh"
#include "D3D12Resource.hh"
#include "D3D12Shader.hh"

#include <d3dcompiler.h>

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
    static const char *GetFullShaderModel(ShaderStage type)
    {
        ZoneScoped;

        const char *pVSTag = "vs_5_1";
        const char *pPSTag = "ps_5_1";
        const char *pCSTag = "cs_5_1";
        const char *pDSTag = "ds_5_1";
        const char *pHSTag = "hs_5_1";

        switch (type)
        {
            case ShaderStage::Vertex: return pVSTag; break;
            case ShaderStage::Pixel: return pPSTag; break;
            case ShaderStage::Compute: return pCSTag; break;
            case ShaderStage::Domain: return pDSTag; break;
            case ShaderStage::Hull: return pHSTag; break;
            default: break;
        }

        return nullptr;
    }

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
        m_pTypeData = Memory::Allocate<u8>(kTypeMem);

        constexpr u32 kDescriptorMem = Memory::MiBToBytes(1);
        constexpr u32 kBufferLinearMem = Memory::MiBToBytes(64);
        constexpr u32 kBufferTLSFMem = Memory::MiBToBytes(512);
        constexpr u32 kImageTLSFMem = Memory::MiBToBytes(512);

        m_MADescriptor.Allocator.AddPool(kDescriptorMem);
        m_MADescriptor.pHeap = CreateHeap(kDescriptorMem, true);

        m_MABufferLinear.Allocator.AddPool(kBufferLinearMem);
        m_MABufferLinear.pHeap = CreateHeap(kBufferLinearMem, true);

        m_MABufferTLSF.Allocator.Init(kBufferTLSFMem);
        m_MABufferTLSF.pHeap = CreateHeap(kBufferTLSFMem, false);

        m_MAImageTLSF.Allocator.Init(kImageTLSFMem);
        m_MAImageTLSF.pHeap = CreateHeap(kImageTLSFMem, false);
    }

    BaseCommandQueue *D3D12API::CreateCommandQueue(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandQueue *pQueue = AllocTypeInherit<BaseCommandQueue, D3D12CommandQueue>();

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

        D3D12CommandAllocator *pAllocator = AllocTypeInherit<BaseCommandAllocator, D3D12CommandAllocator>();

        m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pAllocator->pHandle));

        return pAllocator;
    }

    BaseCommandList *D3D12API::CreateCommandList(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandList *pList = AllocTypeInherit<BaseCommandList, D3D12CommandList>();
        BaseCommandAllocator *pAllocator = CreateCommandAllocator(type);

        API_VAR(D3D12CommandAllocator, pAllocator);

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

        ID3D12DescriptorHeap *pHeaps[] = {
            m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].pHandle,
            // m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].pHandle,
        };

        pListDX->m_pHandle->SetDescriptorHeaps(1, pHeaps);
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

        return completed >= desired;
    }

    ID3D12PipelineLibrary *D3D12API::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
    {
        ZoneScoped;

        ID3D12PipelineLibrary *pLibrary = nullptr;

        m_pDevice->CreatePipelineLibrary(pInitialData, initialDataSize, IID_PPV_ARGS(&pLibrary));

        return pLibrary;
    }

    BaseShader *D3D12API::CreateShader(ShaderStage stage, BufferReadStream &buf)
    {
        ZoneScoped;

        D3D12Shader *pShader = AllocTypeInherit<BaseShader, D3D12Shader>();

        u32 flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

        ID3DBlob *pCode = nullptr;
        ID3DBlob *pError = nullptr;
        HRESULT hr = D3DCompile((LPCVOID)buf.GetData(),
                                buf.GetLength(),
                                nullptr,
                                nullptr,
                                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                "main",
                                GetFullShaderModel(stage),
                                flags,
                                0,
                                &pCode,
                                &pError);

        if (hr < 0)
        {
            LOG_ERROR("Failed to compile shader! DXC says: {}",
                      eastl::string_view((const char *)pError->GetBufferPointer(), pError->GetBufferSize()));

            return nullptr;
        }

        pShader->Type = stage;
        pShader->pHandle = D3D12_SHADER_BYTECODE{ pCode->GetBufferPointer(), pCode->GetBufferSize() };

        return pShader;
    }

    BaseShader *D3D12API::CreateShader(ShaderStage stage, eastl::string_view path)
    {
        ZoneScoped;

        FileStream fs(path, false);
        void *pData = fs.ReadAll<void>();
        fs.Close();

        BufferReadStream buf(pData, fs.Size());
        BaseShader *pShader = CreateShader(stage, buf);

        free(pData);

        return pShader;
    }

    void D3D12API::DeleteShader(BaseShader *pShader)
    {
        ZoneScoped;

        API_VAR(BaseShader, pShader);

        FreeType(pShaderDX);
    }

    GraphicsPipelineBuildInfo *D3D12API::BeginPipelineBuildInfo()
    {
        ZoneScoped;

        GraphicsPipelineBuildInfo *pBuildInfo = new D3D12GraphicsPipelineBuildInfo;
        pBuildInfo->Init();

        return pBuildInfo;
    }

    BasePipeline *D3D12API::EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        API_VAR(D3D12GraphicsPipelineBuildInfo, pBuildInfo);
        D3D12Pipeline *pPipeline = AllocTypeInherit<BasePipeline, D3D12Pipeline>();

        D3D12_ROOT_PARAMETER1 pParams[8] = {};

        for (u32 i = 0; i < pBuildInfo->m_DescriptorSetCount; i++)
        {
            BaseDescriptorSet *pSet = pBuildInfo->m_ppDescriptorSets[i];

            for (u32 j = 0; j < pSet->BindingCount; j++)
            {
                pParams[i].ParameterType = ToDXDescriptorType(pSet->pBindingInfos[j].Type);
                pParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // TODO

                pParams[i].Descriptor.RegisterSpace = i;
                pParams[i].Descriptor.ShaderRegister = j;
                pParams[i].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.NumParameters = pBuildInfo->m_DescriptorSetCount;
        rootSignatureDesc.Desc_1_1.pParameters = pParams;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob *pRootSigBlob = nullptr;
        ID3DBlob *pErrorBlob = nullptr;
        D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &pRootSigBlob, &pErrorBlob);

        m_pDevice->CreateRootSignature(0, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pPipeline->pLayout));
        pBuildInfoDX->m_CreateInfo.pRootSignature = pPipeline->pLayout;

        SAFE_RELEASE(pRootSigBlob);
        SAFE_RELEASE(pErrorBlob);

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

    BaseSwapChain *D3D12API::GetSwapChain()
    {
        return &m_SwapChain;
    }

    void D3D12API::Frame()
    {
        ZoneScoped;

        m_SwapChain.Present();
        m_SwapChain.NextFrame();
    }

    BaseDescriptorSet *D3D12API::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        D3D12DescriptorSet *pDescriptorSet = AllocTypeInherit<BaseDescriptorSet, D3D12DescriptorSet>();

        D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc = {};

        D3D12_ROOT_PARAMETER pBindings[8] = {};

        for (u32 i = 0; i < pDesc->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->pBindings[i];

            pBindings[i].ParameterType = ToDXDescriptorType(element.Type);
            pBindings[i].ShaderVisibility = ToDXShaderType(element.TargetShader);

            // Constant Buffers
            pBindings[i].Descriptor.RegisterSpace = 0;
            pBindings[i].Descriptor.ShaderRegister = i;

            pBindings[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        rootSignatureDesc.Desc_1_0.NumParameters = pDesc->BindingCount;
        rootSignatureDesc.Desc_1_0.pParameters = pBindings;

        ID3DBlob *pRootSigBlob = nullptr;
        ID3DBlob *pErrorBlob = nullptr;
        D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &pRootSigBlob, &pErrorBlob);

        m_pDevice->CreateRootSignature(0, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pDescriptorSet->m_pHandle));

        SAFE_RELEASE(pRootSigBlob);
        SAFE_RELEASE(pErrorBlob);

        /// ---------------------------------------------------------- ///

        pDescriptorSet->BindingCount = pDesc->BindingCount;
        memcpy(pDescriptorSet->pBindingInfos, pDesc->pBindings, pDesc->BindingCount * sizeof(DescriptorBindingDesc));

        return pDescriptorSet;
    }

    void D3D12API::UpdateDescriptorData(BaseDescriptorSet *pSet)
    {
        ZoneScoped;

        API_VAR(D3D12DescriptorSet, pSet);

        u32 bufferIndex = 0;
        u32 imageIndex = 0;

        for (u32 i = 0; i < pSetDX->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pSetDX->pBindingInfos[i];

            BaseBuffer *pBuffer = element.pBuffer;
            BaseImage *pImage = element.pImage;

            API_VAR(D3D12Buffer, pBuffer);
            API_VAR(D3D12Image, pImage);

            if (element.Type == DescriptorType::ConstantBufferView)
            {
                D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
                viewDesc.BufferLocation = pBufferDX->m_pHandle->GetGPUVirtualAddress();
                viewDesc.SizeInBytes = pBufferDX->m_DataSize;

                pBufferDX->m_ViewHandle = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                pBufferDX->m_DeviceHandle = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

                m_pDevice->CreateConstantBufferView(&viewDesc, pBufferDX->m_ViewHandle);
                pSetDX->m_pDescriptorHandle[bufferIndex] = viewDesc.BufferLocation;
                pSetDX->m_DescriptorCount++;
                bufferIndex++;
            }
        }
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
            heapDesc.Flags = element.HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                                                        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

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
            handle.ptr += heap.IncrementSize * heap.GPUOffset;
            heap.GPUOffset++;
        }
        else
        {
            handle.ptr += heap.IncrementSize * index;
        }

        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12API::AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ZoneScoped;

        D3D12DescriptorHeap &heap = m_pDescriptorHeaps[type];

        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.pHandle->GetGPUDescriptorHandleForHeapStart();

        if (index == -1)
        {
            handle.ptr += heap.IncrementSize * heap.GPUOffset;
            heap.GPUOffset++;
        }
        else
        {
            handle.ptr += heap.IncrementSize * index;
        }

        return handle;
    }

    ID3D12Heap *D3D12API::CreateHeap(u64 heapSize, bool cpuWrite)
    {
        ZoneScoped;

        D3D12_HEAP_DESC heapDesc = {};
        heapDesc.SizeInBytes = heapSize;
        heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDesc.Properties.Type = cpuWrite ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
        heapDesc.Properties.VisibleNodeMask = 0;
        heapDesc.Properties.CreationNodeMask = 0;

        ID3D12Heap *pHandle = nullptr;
        m_pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&pHandle));

        return pHandle;
    }

    BaseBuffer *D3D12API::CreateBuffer(BufferDesc *pDesc, BufferData *pData)
    {
        ZoneScoped;

        D3D12Buffer *pBuffer = AllocTypeInherit<BaseBuffer, D3D12Buffer>();

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resourceDesc.Width = pData->DataLen;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_ALLOCATION_INFO memoryInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);

        /// ---------------------------------------------- ///

        pBuffer->m_RequiredDataSize = memoryInfo.SizeInBytes;
        pBuffer->m_Alignment = memoryInfo.Alignment;
        pBuffer->m_Usage = pDesc->UsageFlags;
        pBuffer->m_Mappable = pDesc->Mappable;
        pBuffer->m_DataSize = pData->DataLen;
        pBuffer->m_AllocatorType = pDesc->TargetAllocator;
        pBuffer->m_Stride = pData->Stride;

        /// ---------------------------------------------- ///

        switch (pDesc->TargetAllocator)
        {
            case AllocatorType::None:
            {
                pBuffer->m_DataOffset = 0;
                pBuffer->m_pMemoryHandle = CreateHeap(memoryInfo.SizeInBytes, pBuffer->m_Mappable);

                break;
            }

            case AllocatorType::Descriptor:
            {
                pBuffer->m_DataOffset = m_MADescriptor.Allocator.Allocate(0, pBuffer->m_RequiredDataSize, pBuffer->m_Alignment);
                pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

                break;
            }

            case AllocatorType::BufferLinear:
            {
                pBuffer->m_DataOffset = m_MABufferLinear.Allocator.Allocate(0, pBuffer->m_RequiredDataSize, pBuffer->m_Alignment);
                pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

                break;
            }

            case AllocatorType::BufferTLSF:
            {
                Memory::TLSFBlock *pBlock = m_MABufferTLSF.Allocator.Allocate(pBuffer->m_RequiredDataSize, pBuffer->m_Alignment);

                pBuffer->m_pAllocatorData = pBlock;
                pBuffer->m_DataOffset = pBlock->Offset;

                pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

                break;
            }

            default: break;
        }

        D3D12_RESOURCE_STATES initialState = (pBuffer->m_Mappable ? D3D12_RESOURCE_STATE_GENERIC_READ : ToDXBufferUsage(pBuffer->m_Usage));
        m_pDevice->CreatePlacedResource(pBuffer->m_pMemoryHandle,
                                        pBuffer->m_DataOffset,
                                        &resourceDesc,
                                        initialState,
                                        nullptr,
                                        IID_PPV_ARGS(&pBuffer->m_pHandle));

        return pBuffer;
    }

    void D3D12API::DeleteBuffer(BaseBuffer *pHandle)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pHandle);

        if (pHandleDX->m_pMemoryHandle)
        {
            if (pHandleDX->m_AllocatorType == AllocatorType::BufferTLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pHandleDX->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MABufferTLSF.Allocator.Free(pBlock);
            }
            else if (pHandleDX->m_AllocatorType == AllocatorType::None)
            {
                pHandleDX->m_pMemoryHandle->Release();
            }
        }

        if (pHandleDX->m_pHandle)
            pHandleDX->m_pHandle->Release();
    }

    void D3D12API::MapMemory(BaseBuffer *pBuffer, void *&pData)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        pBufferDX->m_pHandle->Map(0, nullptr, &pData);
    }

    void D3D12API::UnmapMemory(BaseBuffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        pBufferDX->m_pHandle->Unmap(0, nullptr);
    }

    BaseBuffer *D3D12API::ChangeAllocator(BaseCommandList *pList, BaseBuffer *pTarget, AllocatorType targetAllocator)
    {
        ZoneScoped;

        // TODO: Currently this technique uses present queue, move it to transfer queue

        BufferDesc bufDesc = {};
        BufferData bufData = {};

        bufDesc.Mappable = false;
        bufDesc.UsageFlags = ResourceUsage::CopyDst;
        bufDesc.UsageFlags &= ~ResourceUsage::CopySrc;
        bufDesc.TargetAllocator = targetAllocator;

        bufData.Stride = pTarget->m_Stride;
        bufData.DataLen = pTarget->m_DataSize;

        BaseBuffer *pNewBuffer = CreateBuffer(&bufDesc, &bufData);

        pList->CopyBuffer(pTarget, pNewBuffer, bufData.DataLen);

        pNewBuffer->m_Usage &= ~ResourceUsage::CopyDst;
        pNewBuffer->m_Usage |= pTarget->m_Usage;

        pList->BarrierTransition(pNewBuffer, ResourceUsage::CopyDst, ShaderStage::None, pTarget->m_Usage, ShaderStage::None);

        return pNewBuffer;
    }

    BaseImage *D3D12API::CreateImage(ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        D3D12Image *pImage = AllocTypeInherit<BaseImage, D3D12Image>();

        /// ---------------------------------------------- ///
        /// ---------------------------------------------- ///

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

    void D3D12API::CreateSampler(BaseImage *pHandle)
    {
        ZoneScoped;
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

                mask ^= 1 << index;
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

    constexpr D3D12_DESCRIPTOR_HEAP_TYPE kDescriptorHeapTypeLUT[] = {
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,      // Sampler
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // ShaderResourceView
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // ConstantBufferView
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // UnorderedAccessBuffer
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  // UnorderedAccessView
        D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES,    // RootConstant
    };

    constexpr D3D12_DESCRIPTOR_RANGE_TYPE kDescriptorRangeLUT[] = {
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,  // Sampler
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,      // ShaderResourceView
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV,      // ConstantBufferView
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,      // UnorderedAccessBuffer
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,      // UnorderedAccessView
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
        D3D12_ROOT_PARAMETER_TYPE parameter;

        switch (type)
        {
            case DescriptorType::ConstantBufferView: return D3D12_ROOT_PARAMETER_TYPE_CBV;
            case DescriptorType::ShaderResourceView: return D3D12_ROOT_PARAMETER_TYPE_SRV;
            case DescriptorType::UnorderedAccessBuffer:
            case DescriptorType::UnorderedAccessView: return D3D12_ROOT_PARAMETER_TYPE_UAV;
            case DescriptorType::RootConstant: return D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            default: assert(!"Unhandled descriptor type"); return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }
    }

    D3D12_DESCRIPTOR_HEAP_TYPE D3D12API::ToDXHeapType(DescriptorType type)
    {
        return kDescriptorHeapTypeLUT[(u32)type];
    }

    D3D12_DESCRIPTOR_RANGE_TYPE D3D12API::ToDXDescriptorRangeType(DescriptorType type)
    {
        return kDescriptorRangeLUT[(u32)type];
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXImageUsage(ResourceUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage & ResourceUsage::Undefined)
            v |= D3D12_RESOURCE_STATE_COMMON;

        if (usage & ResourceUsage::RenderTarget)
            v |= D3D12_RESOURCE_STATE_RENDER_TARGET;

        if (usage & ResourceUsage::DepthStencilWrite)
            v |= D3D12_RESOURCE_STATE_DEPTH_WRITE;

        if (usage & ResourceUsage::DepthStencilRead)
            v |= D3D12_RESOURCE_STATE_DEPTH_READ;

        if (usage & ResourceUsage::ShaderResource)
            v |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        if (usage & ResourceUsage::CopySrc)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & ResourceUsage::CopyDst)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        if (usage & ResourceUsage::Present)
            v |= D3D12_RESOURCE_STATE_PRESENT;

        return v;
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXBufferUsage(ResourceUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage & ResourceUsage::VertexBuffer)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

        if (usage & ResourceUsage::IndexBuffer)
            v |= D3D12_RESOURCE_STATE_INDEX_BUFFER;

        if (usage & ResourceUsage::ConstantBuffer)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

        if (usage & ResourceUsage::UnorderedAccess)
            v |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        if (usage & ResourceUsage::IndirectBuffer)
            v |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

        if (usage & ResourceUsage::CopySrc)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & ResourceUsage::CopyDst)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        return v;
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXImageLayout(ResourceUsage usage)
    {
        return ToDXImageUsage(usage);
    }

    D3D12_SHADER_VISIBILITY D3D12API::ToDXShaderType(ShaderStage type)
    {
        return kShaderTypeLUT[(u32)type];
    }

}  // namespace lr::Graphics