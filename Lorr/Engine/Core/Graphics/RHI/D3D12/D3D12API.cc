#include "D3D12API.hh"

#include "D3D12Pipeline.hh"
#include "D3D12Resource.hh"
#include "D3D12Shader.hh"

#include "STL/Memory.hh"

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
    bool D3D12API::Init(BaseWindow *pWindow, u32 width, u32 height, APIFlags flags)
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

        m_TypeAllocator.Init(kTypeMem, 0x2000);
        m_pTypeData = Memory::Allocate<u8>(kTypeMem);

        constexpr u32 kDescriptorMem = Memory::MiBToBytes(1);
        constexpr u32 kBufferLinearMem = Memory::MiBToBytes(64);
        constexpr u32 kBufferTLSFMem = Memory::MiBToBytes(512);
        constexpr u32 kImageTLSFMem = Memory::MiBToBytes(512);

        constexpr D3D12_HEAP_FLAGS kBufferFlags = D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
        constexpr D3D12_HEAP_FLAGS kImageFlags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS;

        m_MADescriptor.Allocator.Init(kDescriptorMem);
        m_MADescriptor.pHeap = CreateHeap(kDescriptorMem, true, kBufferFlags);

        m_MABufferLinear.Allocator.Init(kBufferLinearMem);
        m_MABufferLinear.pHeap = CreateHeap(kBufferLinearMem, true, kBufferFlags);

        m_MABufferTLSF.Allocator.Init(kBufferTLSFMem, 0x2000);
        m_MABufferTLSF.pHeap = CreateHeap(kBufferTLSFMem, false, kBufferFlags);

        m_MAImageTLSF.Allocator.Init(kImageTLSFMem, 0x2000);
        m_MAImageTLSF.pHeap = CreateHeap(kImageTLSFMem, false, kImageFlags);
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

        pShader->Type = stage;

        ShaderCompileDesc compileDesc = {};
        compileDesc.Type = stage;
        compileDesc.Flags = LR_SHADER_FLAG_MATRIX_ROW_MAJOR | LR_SHADER_FLAG_ALL_RESOURCES_BOUND;

#if _DEBUG
        compileDesc.Flags |= LR_SHADER_FLAG_WARNINGS_ARE_ERRORS | LR_SHADER_FLAG_SKIP_OPTIMIZATION | LR_SHADER_FLAG_USE_DEBUG;
#endif

        pShader->pHandle = ShaderCompiler::CompileFromBuffer(&compileDesc, buf);

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

        u32 currentSpace = 0;

        D3D12_ROOT_PARAMETER1 pParams[16] = {};  // 8 descriptor + 8 root constants
        D3D12_DESCRIPTOR_RANGE1 pRanges[8][8] = {};

        D3D12_STATIC_SAMPLER_DESC pSamplers[8] = {};

        for (u32 i = 0; i < pBuildInfo->m_DescriptorSetCount; i++)
        {
            BaseDescriptorSet *pSet = pBuildInfo->m_ppDescriptorSets[i];
            API_VAR(D3D12DescriptorSet, pSet);

            pParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            pParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            pParams[i].DescriptorTable.NumDescriptorRanges = pSetDX->BindingCount;
            pParams[i].DescriptorTable.pDescriptorRanges = pRanges[i];

            for (u32 j = 0; j < pSetDX->BindingCount; j++)
            {
                D3D12_DESCRIPTOR_RANGE1 &range = pRanges[i][j];

                range.RegisterSpace = currentSpace;
                range.BaseShaderRegister = j;
                range.NumDescriptors = 1;
                range.RangeType = ToDXDescriptorRangeType(pSetDX->pBindings[j].Type);
                range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            }

            currentSpace++;
        }

        u32 samplerCount = 0;
        if (pBuildInfo->m_pSamplerDescriptorSet)
        {
            BaseDescriptorSet *pSet = pBuildInfo->m_pSamplerDescriptorSet;
            API_VAR(D3D12DescriptorSet, pSet);

            samplerCount = pSetDX->BindingCount;

            for (u32 i = 0; i < pSetDX->BindingCount; i++)
            {
                D3D12_STATIC_SAMPLER_DESC &sampler = pSamplers[i];
                DescriptorBindingDesc &binding = pSetDX->pBindings[i];
                SamplerDesc &samplerDesc = binding.Sampler;

                sampler.ShaderRegister = binding.BindingID;
                sampler.RegisterSpace = currentSpace++;
                sampler.ShaderVisibility = ToDXShaderType(binding.TargetShader);

                CreateSampler(&samplerDesc, sampler);
            }
        }

        for (u32 i = 0; i < pBuildInfo->m_PushConstantCount; i++)
        {
            D3D12_ROOT_PARAMETER1 &param = pParams[i + pBuildInfo->m_DescriptorSetCount];
            PushConstantDesc &pushConstant = pBuildInfo->m_pPushConstants[i];

            param.ShaderVisibility = ToDXShaderType(pushConstant.Stage);
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

            param.Constants.RegisterSpace = currentSpace++;
            param.Constants.ShaderRegister = 0;
            param.Constants.Num32BitValues = pushConstant.Size / sizeof(u32);

            pPipeline->m_pRootConstats[(u32)param.ShaderVisibility] = i + pBuildInfo->m_DescriptorSetCount;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.NumParameters = pBuildInfo->m_DescriptorSetCount + pBuildInfo->m_PushConstantCount;
        rootSignatureDesc.Desc_1_1.pParameters = pParams;
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = samplerCount;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = pSamplers;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ls::scoped_comptr<ID3DBlob> pRootSigBlob;
        ls::scoped_comptr<ID3DBlob> pErrorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, pRootSigBlob.get_address(), pErrorBlob.get_address());

        if (pErrorBlob.get() && pErrorBlob->GetBufferPointer())
            LOG_ERROR("Root signature error: {}", eastl::string_view((const char *)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize()));

        m_pDevice->CreateRootSignature(0, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pPipeline->pLayout));
        pBuildInfoDX->m_CreateInfo.pRootSignature = pPipeline->pLayout;

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

    void D3D12API::BeginFrame()
    {
        ZoneScoped;
    }

    void D3D12API::EndFrame()
    {
        ZoneScoped;

        m_SwapChain.Present();
        m_SwapChain.NextFrame();
    }

    BaseDescriptorSet *D3D12API::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        D3D12DescriptorSet *pDescriptorSet = AllocTypeInherit<BaseDescriptorSet, D3D12DescriptorSet>();
        pDescriptorSet->BindingCount = pDesc->BindingCount;
        memcpy(pDescriptorSet->pBindings, pDesc->pBindings, pDesc->BindingCount * sizeof(DescriptorBindingDesc));

        D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc = {};

        u32 bindingCount = 0;
        D3D12_ROOT_PARAMETER pBindings[8] = {};

        for (u32 i = 0; i < pDesc->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->pBindings[i];

            if (element.Type == DescriptorType::Sampler)
            {
                continue;
            }

            D3D12_ROOT_PARAMETER &binding = pBindings[bindingCount];

            binding.ParameterType = ToDXDescriptorType(element.Type);
            binding.ShaderVisibility = ToDXShaderType(element.TargetShader);
            binding.Descriptor.RegisterSpace = 0;
            binding.Descriptor.ShaderRegister = element.BindingID;

            bindingCount++;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        rootSignatureDesc.Desc_1_0.NumParameters = bindingCount;
        rootSignatureDesc.Desc_1_0.pParameters = pBindings;

        ls::scoped_comptr<ID3DBlob> pRootSigBlob;
        ls::scoped_comptr<ID3DBlob> pErrorBlob;
        D3D12SerializeVersionedRootSignature(&rootSignatureDesc, pRootSigBlob.get_address(), pErrorBlob.get_address());

        if (pErrorBlob.get() && pErrorBlob->GetBufferPointer())
            LOG_ERROR("Root signature error: {}", eastl::string_view((const char *)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize()));

        m_pDevice->CreateRootSignature(0, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pDescriptorSet->pHandle));

        return pDescriptorSet;
    }

    void D3D12API::UpdateDescriptorData(BaseDescriptorSet *pSet, DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        API_VAR(D3D12DescriptorSet, pSet);

        for (u32 i = 0; i < pDesc->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->pBindings[i];

            BaseBuffer *pBuffer = element.pBuffer;
            BaseImage *pImage = element.pImage;

            API_VAR(D3D12Buffer, pBuffer);
            API_VAR(D3D12Image, pImage);

            D3D12_GPU_DESCRIPTOR_HANDLE &gpuAddress = pSetDX->pDescriptorHandles[i];

            switch (element.Type)
            {
                case DescriptorType::ConstantBufferView: gpuAddress = pBufferDX->m_ViewGPU; break;
                case DescriptorType::ShaderResourceView: gpuAddress = pImageDX->m_ShaderViewGPU; break;
                default: break;
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
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (element.HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || element.HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

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
            handle.ptr += heap.IncrementSize * heap.CPUOffset;
            heap.CPUOffset++;
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

    ID3D12Heap *D3D12API::CreateHeap(u64 heapSize, bool cpuWrite, D3D12_HEAP_FLAGS flags)
    {
        ZoneScoped;

        D3D12_HEAP_DESC heapDesc = {};
        heapDesc.SizeInBytes = heapSize;
        heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        heapDesc.Flags = flags;

        heapDesc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapDesc.Properties.VisibleNodeMask = 0;
        heapDesc.Properties.CreationNodeMask = 0;
        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L1;

        if (cpuWrite)
        {
            heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        }

        ID3D12Heap *pHandle = nullptr;
        m_pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&pHandle));

        return pHandle;
    }

    BaseBuffer *D3D12API::CreateBuffer(BufferDesc *pDesc, BufferData *pData)
    {
        ZoneScoped;

        D3D12Buffer *pBuffer = AllocTypeInherit<BaseBuffer, D3D12Buffer>();
        pBuffer->m_Usage = pDesc->UsageFlags;
        pBuffer->m_Mappable = pDesc->Mappable;
        pBuffer->m_DataSize = pData->DataLen;
        pBuffer->m_AllocatorType = pDesc->TargetAllocator;
        pBuffer->m_Stride = pData->Stride;

        /// ---------------------------------------------- ///

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

        /// ---------------------------------------------- ///

        SetAllocator(pBuffer, resourceDesc, pDesc->TargetAllocator);

        m_pDevice->CreatePlacedResource(pBuffer->m_pMemoryHandle,
                                        pBuffer->m_DataOffset,
                                        &resourceDesc,
                                        ToDXBufferUsage(pBuffer->m_Usage),
                                        nullptr,
                                        IID_PPV_ARGS(&pBuffer->m_pHandle));

        pBuffer->m_VirtualAddress = pBuffer->m_pHandle->GetGPUVirtualAddress();

        CreateBufferView(pBuffer);

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

    void D3D12API::CreateBufferView(BaseBuffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        if (pBuffer->m_Usage & ResourceUsage::ConstantBuffer)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
            viewDesc.BufferLocation = pBufferDX->m_VirtualAddress;
            viewDesc.SizeInBytes = pBufferDX->m_DataSize;

            pBufferDX->m_ViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            pBufferDX->m_ViewGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_pDevice->CreateConstantBufferView(&viewDesc, pBufferDX->m_ViewCPU);
        }
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
        bufDesc.TargetAllocator = targetAllocator;

        bufData.Stride = pTarget->m_Stride;
        bufData.DataLen = pTarget->m_DataSize;

        BaseBuffer *pNewBuffer = CreateBuffer(&bufDesc, &bufData);

        pList->CopyBuffer(pTarget, pNewBuffer, bufData.DataLen);

        pNewBuffer->m_Usage = pTarget->m_Usage;
        pNewBuffer->m_Usage &= ~ResourceUsage::CopySrc;

        pList->BarrierTransition(pNewBuffer, ResourceUsage::CopyDst, ShaderStage::None, pTarget->m_Usage, ShaderStage::None);

        return pNewBuffer;
    }

    BaseImage *D3D12API::CreateImage(ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        D3D12Image *pImage = AllocTypeInherit<BaseImage, D3D12Image>();
        pImage->m_Width = pData->Width;
        pImage->m_Height = pData->Height;
        pImage->m_DataSize = pData->DataLen;
        pImage->m_UsingMip = 0;
        pImage->m_TotalMips = pDesc->MipMapLevels;
        pImage->m_Usage = pDesc->Usage;
        pImage->m_Format = pDesc->Format;
        pImage->m_AllocatorType = pDesc->TargetAllocator;

        /// ---------------------------------------------- ///

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        if (pDesc->Usage & ResourceUsage::RenderTarget)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (pDesc->Usage & ResourceUsage::DepthStencilWrite || pDesc->Usage & ResourceUsage::DepthStencilRead)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (pDesc->Usage & ResourceUsage::UnorderedAccess)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = pData->Width;
        resourceDesc.Height = pData->Height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = pDesc->MipMapLevels;
        resourceDesc.Format = ToDXFormat(pDesc->Format);
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = flags;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;

        /// ---------------------------------------------- ///

        SetAllocator(pImage, resourceDesc, pDesc->TargetAllocator);

        m_pDevice->CreatePlacedResource(pImage->m_pMemoryHandle,
                                        pImage->m_DataOffset,
                                        &resourceDesc,
                                        ToDXImageUsage(pImage->m_Usage),
                                        nullptr,
                                        IID_PPV_ARGS(&pImage->m_pHandle));

        CreateImageView(pImage);

        if (pData->pData)
        {
            BufferDesc bufferDesc = {};
            bufferDesc.Mappable = true;
            bufferDesc.UsageFlags = ResourceUsage::CopySrc;
            bufferDesc.TargetAllocator = AllocatorType::BufferFrameTime;

            BufferData bufferData = {};
            bufferData.DataLen = pData->DataLen;

            BaseBuffer *pImageBuffer = CreateBuffer(&bufferDesc, &bufferData);

            void *pMapData = nullptr;
            MapMemory(pImageBuffer, pMapData);
            memcpy(pMapData, pData->pData, pData->DataLen);
            UnmapMemory(pImageBuffer);

            BaseCommandList *pList = GetCommandList();
            BeginCommandList(pList);

            pList->BarrierTransition(pImage, ResourceUsage::ShaderResource, ShaderStage::None, ResourceUsage::CopyDst, ShaderStage::None);
            pList->CopyBuffer(pImageBuffer, pImage);
            pList->BarrierTransition(pImage, ResourceUsage::CopyDst, ShaderStage::None, ResourceUsage::ShaderResource, ShaderStage::Pixel);

            EndCommandList(pList);
            ExecuteCommandList(pList, true);

            DeleteBuffer(pImageBuffer);
        }

        return pImage;
    }

    void D3D12API::DeleteImage(BaseImage *pImage)
    {
        ZoneScoped;
    }

    void D3D12API::CreateImageView(BaseImage *pImage)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        if (pImageDX->m_Usage & ResourceUsage::ShaderResource)
        {
            if (pImageDX->m_ShaderViewCPU.ptr == 0)
                pImageDX->m_ShaderViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            if (pImageDX->m_ShaderViewGPU.ptr == 0)
                pImageDX->m_ShaderViewGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImageDX->m_Format);
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            viewDesc.Texture2D.MostDetailedMip = 0;
            viewDesc.Texture2D.MipLevels = pImage->m_TotalMips;
            viewDesc.Texture2D.PlaneSlice = 0;
            viewDesc.Texture2D.ResourceMinLODClamp = 0;

            m_pDevice->CreateShaderResourceView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_ShaderViewCPU);
        }

        if (pImageDX->m_Usage & ResourceUsage::RenderTarget)
        {
            pImageDX->m_RenderTargetViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImage->m_Format);
            viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice = 0;

            m_pDevice->CreateRenderTargetView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_RenderTargetViewCPU);
        }

        if (pImageDX->m_Usage & ResourceUsage::DepthStencilWrite || pImageDX->m_Usage & ResourceUsage::DepthStencilRead)
        {
            pImageDX->m_DepthStencilViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

            D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImage->m_Format);
            viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice = 0;

            if (pImageDX->m_Usage & ResourceUsage::DepthStencilRead)
                viewDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;

            m_pDevice->CreateDepthStencilView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_DepthStencilViewCPU);
        }
    }

    void D3D12API::CreateSampler(SamplerDesc *pDesc, D3D12_STATIC_SAMPLER_DESC &samplerOut)
    {
        ZoneScoped;

        constexpr static D3D12_TEXTURE_ADDRESS_MODE kAddresModeLUT[] = {
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        };

        u32 minFilterVal = (u32)pDesc->MinFilter;
        u32 magFilterVal = (u32)pDesc->MagFilter;
        u32 mipFilterVal = (u32)pDesc->MipFilter;

        samplerOut.Filter = (D3D12_FILTER)(mipFilterVal | (magFilterVal << 2) | (minFilterVal << 4) | (pDesc->UseAnisotropy << 6));

        samplerOut.AddressU = kAddresModeLUT[(u32)pDesc->AddressU];
        samplerOut.AddressV = kAddresModeLUT[(u32)pDesc->AddressV];
        samplerOut.AddressW = kAddresModeLUT[(u32)pDesc->AddressW];

        samplerOut.MaxAnisotropy = pDesc->MaxAnisotropy;

        samplerOut.MipLODBias = pDesc->MipLODBias;
        samplerOut.MaxLOD = pDesc->MaxLOD;
        samplerOut.MinLOD = pDesc->MinLOD;

        samplerOut.ComparisonFunc = (D3D12_COMPARISON_FUNC)pDesc->ComparisonFunc;

        samplerOut.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    }

    void D3D12API::SetAllocator(D3D12Buffer *pBuffer, D3D12_RESOURCE_DESC &resourceDesc, AllocatorType targetAllocator)
    {
        ZoneScoped;

        D3D12_RESOURCE_ALLOCATION_INFO memoryInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);
        pBuffer->m_RequiredDataSize = memoryInfo.SizeInBytes;

        switch (targetAllocator)
        {
            case AllocatorType::None:
            {
                pBuffer->m_DataOffset = 0;
                pBuffer->m_pMemoryHandle = CreateHeap(pBuffer->m_RequiredDataSize, pBuffer->m_Mappable, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);

                break;
            }

            case AllocatorType::Descriptor:
            {
                pBuffer->m_DataOffset = m_MADescriptor.Allocator.Allocate(pBuffer->m_RequiredDataSize, pBuffer->m_Alignment);
                pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

                break;
            }

            case AllocatorType::BufferLinear:
            {
                pBuffer->m_DataOffset = m_MABufferLinear.Allocator.Allocate(pBuffer->m_RequiredDataSize, pBuffer->m_Alignment);
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
    }

    void D3D12API::SetAllocator(D3D12Image *pImage, D3D12_RESOURCE_DESC &resourceDesc, AllocatorType targetAllocator)
    {
        ZoneScoped;

        D3D12_RESOURCE_ALLOCATION_INFO memoryInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);
        pImage->m_RequiredDataSize = memoryInfo.SizeInBytes;

        switch (targetAllocator)
        {
            case AllocatorType::None:
            {
                pImage->m_DataOffset = 0;
                pImage->m_pMemoryHandle = CreateHeap(memoryInfo.SizeInBytes, false, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);

                break;
            }

            case AllocatorType::ImageTLSF:
            {
                Memory::TLSFBlock *pBlock = m_MAImageTLSF.Allocator.Allocate(pImage->m_RequiredDataSize, memoryInfo.Alignment);

                pImage->m_pAllocatorData = pBlock;
                pImage->m_DataOffset = pBlock->Offset;

                pImage->m_pMemoryHandle = m_MAImageTLSF.pHeap;

                break;
            }

            default: break;
        }
    }

    void D3D12API::CalcOrthoProjection(XMMATRIX &mat, XMFLOAT2 viewSize, float zFar, float zNear)
    {
        ZoneScoped;

        mat = XMMatrixOrthographicOffCenterLH(0.0, viewSize.x, viewSize.y, 0.0, zNear, zFar);
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

    constexpr D3D12_ROOT_PARAMETER_TYPE kDescriptorTypeLUT[] = {
        D3D12_ROOT_PARAMETER_TYPE_SRV,              // Sampler
        D3D12_ROOT_PARAMETER_TYPE_SRV,              // ShaderResourceView
        D3D12_ROOT_PARAMETER_TYPE_CBV,              // ConstantBufferView
        D3D12_ROOT_PARAMETER_TYPE_UAV,              // UnorderedAccessBuffer
        D3D12_ROOT_PARAMETER_TYPE_UAV,              // UnorderedAccessView
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,  // RootConstant
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

    D3D12_RESOURCE_STATES D3D12API::ToDXImageUsage(ResourceUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage == ResourceUsage::Present)
            return D3D12_RESOURCE_STATE_PRESENT;

        if (usage == ResourceUsage::Undefined)
            return D3D12_RESOURCE_STATE_COMMON;

        if (usage & ResourceUsage::RenderTarget)
            v |= D3D12_RESOURCE_STATE_RENDER_TARGET;

        if (usage & ResourceUsage::DepthStencilWrite)
            v |= D3D12_RESOURCE_STATE_DEPTH_WRITE;

        if (usage & ResourceUsage::DepthStencilRead)
            v |= D3D12_RESOURCE_STATE_DEPTH_READ;

        if (usage & ResourceUsage::ShaderResource)
            v |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

        if (usage & ResourceUsage::CopySrc)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & ResourceUsage::CopyDst)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

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

        if (usage & ResourceUsage::CopySrc || usage & ResourceUsage::HostRead)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & ResourceUsage::CopyDst || usage & ResourceUsage::HostWrite)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        return v;
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXImageLayout(ResourceUsage usage)
    {
        return ToDXImageUsage(usage);
    }

    D3D12_SHADER_VISIBILITY D3D12API::ToDXShaderType(ShaderStage type)
    {
        u32 v = {};

        if (type & ShaderStage::Vertex)
            v |= D3D12_SHADER_VISIBILITY_VERTEX;

        if (type & ShaderStage::Pixel)
            v |= D3D12_SHADER_VISIBILITY_PIXEL;

        if (type & ShaderStage::Compute)
            v |= D3D12_SHADER_VISIBILITY_ALL;

        if (type & ShaderStage::Hull)
            v |= D3D12_SHADER_VISIBILITY_HULL;

        if (type & ShaderStage::Domain)
            v |= D3D12_SHADER_VISIBILITY_DOMAIN;

        if (type & ShaderStage::Geometry)
            v |= D3D12_SHADER_VISIBILITY_GEOMETRY;

        return (D3D12_SHADER_VISIBILITY)v;
    }

}  // namespace lr::Graphics