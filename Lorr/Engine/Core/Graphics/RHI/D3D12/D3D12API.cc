#include "D3D12API.hh"

#include "D3D12Pipeline.hh"
#include "D3D12Resource.hh"
#include "D3D12Shader.hh"

#include "STL/Memory.hh"

#undef LOG_SET_NAME
#define LOG_SET_NAME "D3D12API"

#define HRCall(func, message)                                                                            \
    if (FAILED(hr = func))                                                                               \
    {                                                                                                    \
        char *pError;                                                                                    \
        FormatMessage(                                                                                   \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
            NULL,                                                                                        \
            hr,                                                                                          \
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
            (LPTSTR)&pError,                                                                             \
            0,                                                                                           \
            NULL);                                                                                       \
        LOG_ERROR(message " Details: {}", pError);                                                       \
        return;                                                                                          \
    }

#define HRCallRet(func, message, ret)                                                                    \
    if (FAILED(hr = func))                                                                               \
    {                                                                                                    \
        char *pError;                                                                                    \
        FormatMessage(                                                                                   \
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
            NULL,                                                                                        \
            hr,                                                                                          \
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
            (LPTSTR)&pError,                                                                             \
            0,                                                                                           \
            NULL);                                                                                       \
        LOG_ERROR(message " Details: {}", pError);                                                       \
        return ret;                                                                                      \
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
        HRCallRet(
            CreateDXGIFactory2((deviceFlags), IID_PPV_ARGS(&m_pFactory)),
            "Failed to create DXGI2 Factory!",
            false);

#pragma region Select Factory
        // Select suitable adapter
        IDXGIFactory6 *pFactory6 = nullptr;
        m_pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6));

        if (pFactory6)
        {
            for (u32 i = 0; pFactory6->EnumAdapterByGpuPreference(
                                i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_pAdapter))
                            != DXGI_ERROR_NOT_FOUND;
                 i++)
            {
                DXGI_ADAPTER_DESC1 adapterDesc = {};
                m_pAdapter->GetDesc1(&adapterDesc);

                if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;  // We don't want software adapter

                if (SUCCEEDED(
                        D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
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

                if (SUCCEEDED(
                        D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
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

        HRCallRet(
            D3D12CreateDevice(m_pAdapter, kMinimumFeatureLevel, IID_PPV_ARGS(&m_pDevice)),
            "Failed to create D3D12 Device!",
            false);

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
            { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256 },
            { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256 },
        });

        m_pPipelineCache = CreatePipelineCache();

        m_pDirectQueue = CreateCommandQueue(CommandListType::Direct);
        m_pComputeQueue = CreateCommandQueue(CommandListType::Compute);

        m_SwapChain.Init(pWindow, this, SwapChainFlags::TripleBuffering);
        m_SwapChain.CreateBackBuffers(this);

        for (CommandList *&pList : m_CommandListPool.m_DirectLists)
            pList = CreateCommandList(CommandListType::Direct);

        for (CommandList *&pList : m_CommandListPool.m_ComputeLists)
            pList = CreateCommandList(CommandListType::Compute);

        for (CommandList *&pList : m_CommandListPool.m_CopyLists)
            pList = CreateCommandList(CommandListType::Copy);

        m_FenceThread.Begin(TFFenceWait, this);

        LOG_TRACE("Successfully initialized D3D12 API.");

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

        constexpr D3D12_HEAP_FLAGS kBufferFlags =
            D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
        constexpr D3D12_HEAP_FLAGS kImageFlags =
            D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS;

        m_MADescriptor.Allocator.Init(kDescriptorMem);
        m_MADescriptor.pHeap = CreateHeap(kDescriptorMem, true, kBufferFlags);

        m_MABufferLinear.Allocator.Init(kBufferLinearMem);
        m_MABufferLinear.pHeap = CreateHeap(kBufferLinearMem, true, kBufferFlags);

        m_MABufferTLSF.Allocator.Init(kBufferTLSFMem, 0x2000);
        m_MABufferTLSF.pHeap = CreateHeap(kBufferTLSFMem, false, kBufferFlags);

        m_MAImageTLSF.Allocator.Init(kImageTLSFMem, 0x2000);
        m_MAImageTLSF.pHeap = CreateHeap(kImageTLSFMem, false, kImageFlags);
    }

    D3D12CommandQueue *D3D12API::CreateCommandQueue(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandQueue *pQueue = AllocType<D3D12CommandQueue>();
        pQueue->m_pFence = CreateFence();
        pQueue->m_FenceEvent = CreateEventExA(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = ToDXCommandListType(type);
        queueDesc.NodeMask = 0;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;  // TODO: Multiple Queues
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pQueue->m_pHandle));

        return pQueue;
    }

    D3D12CommandAllocator *D3D12API::CreateCommandAllocator(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandAllocator *pAllocator = AllocTypeInherit<CommandAllocator, D3D12CommandAllocator>();

        m_pDevice->CreateCommandAllocator(ToDXCommandListType(type), IID_PPV_ARGS(&pAllocator->m_pHandle));

        return pAllocator;
    }

    D3D12CommandList *D3D12API::CreateCommandList(CommandListType type)
    {
        ZoneScoped;

        D3D12CommandList *pList = AllocTypeInherit<CommandList, D3D12CommandList>();
        D3D12CommandAllocator *pAllocator = CreateCommandAllocator(type);

        pList->m_pFence = CreateFence();
        pList->m_FenceEvent = CreateEventExA(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        ID3D12GraphicsCommandList4 *pHandle = nullptr;
        m_pDevice->CreateCommandList(
            0, ToDXCommandListType(type), pAllocator->m_pHandle, nullptr, IID_PPV_ARGS(&pHandle));

        pList->Init(pHandle, pAllocator, type);

        return pList;
    }

    void D3D12API::BeginCommandList(CommandList *pList)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        pListDX->m_pAllocator->Reset();
        pListDX->Reset(pListDX->m_pAllocator);

        eastl::array<ID3D12DescriptorHeap *, 2> pHeaps = {
            m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].m_pHandle,
            m_pDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].m_pHandle,
        };

        pListDX->m_pHandle->SetDescriptorHeaps(pHeaps.count, pHeaps.data());
    }

    void D3D12API::EndCommandList(CommandList *pList)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        pListDX->m_pHandle->Close();
    }

    void D3D12API::ResetCommandAllocator(CommandAllocator *pAllocator)
    {
        ZoneScoped;

        API_VAR(D3D12CommandAllocator, pAllocator);

        pAllocatorDX->Reset();
    }

    void D3D12API::ExecuteCommandList(CommandList *pList, bool waitForFence)
    {
        ZoneScoped;

        API_VAR(D3D12CommandList, pList);

        m_pDirectQueue->ExecuteCommandList(pListDX);

        if (waitForFence)
        {
            pListDX->m_pFence->SetEventOnCompletion(
                pListDX->m_DesiredFenceValue.load(eastl::memory_order_acquire), pListDX->m_FenceEvent);
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

    ID3D12RootSignature *D3D12API::SerializeRootSignature(
        PipelineLayoutSerializeDesc *pDesc, Pipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        D3D12_ROOT_PARAMETER1
            pParams[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE + LR_MAX_PUSH_CONSTANTS_PER_PIPELINE] = {};
        D3D12_DESCRIPTOR_RANGE1 pRanges[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE]
                                       [LR_MAX_DESCRIPTORS_PER_LAYOUT] = {};
        D3D12_STATIC_SAMPLER_DESC pSamplers[LR_MAX_STATIC_SAMPLERS_PER_PIPELINE] = {};

        u32 currentSpace = 0;

        for (u32 i = 0; i < pDesc->m_DescriptorSetCount; i++)
        {
            DescriptorSet *pSet = pDesc->m_ppDescriptorSets[i];
            API_VAR(D3D12DescriptorSet, pSet);

            pParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            pParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            pParams[i].DescriptorTable.NumDescriptorRanges = pSetDX->m_BindingCount;
            pParams[i].DescriptorTable.pDescriptorRanges = pRanges[i];

            for (u32 j = 0; j < pSetDX->m_BindingCount; j++)
            {
                D3D12_DESCRIPTOR_RANGE1 &range = pRanges[i][j];

                range.RegisterSpace = currentSpace;
                range.BaseShaderRegister = j;
                range.NumDescriptors = 1;
                range.RangeType = ToDXDescriptorRangeType(pSetDX->m_pBindingTypes[j]);
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                if (range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
            }

            currentSpace++;
        }

        for (u32 i = 0; i < pDesc->m_PushConstantCount; i++)
        {
            u32 paramIdx = i + pDesc->m_DescriptorSetCount;

            D3D12_ROOT_PARAMETER1 &param = pParams[paramIdx];
            PushConstantDesc &pushConstant = pDesc->m_pPushConstants[i];

            param.ShaderVisibility = ToDXShaderType(pushConstant.m_Stage);
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;

            param.Constants.RegisterSpace = currentSpace++;
            param.Constants.ShaderRegister = 0;
            param.Constants.Num32BitValues = pushConstant.m_Size / sizeof(u32);

            pPipelineDX->m_pRootConstats[(u32)param.ShaderVisibility] = paramIdx;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.NumParameters = pDesc->m_DescriptorSetCount + pDesc->m_PushConstantCount;
        rootSignatureDesc.Desc_1_1.pParameters = pParams;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ls::scoped_comptr<ID3DBlob> pRootSigBlob;
        ls::scoped_comptr<ID3DBlob> pErrorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(
            &rootSignatureDesc, pRootSigBlob.get_address(), pErrorBlob.get_address());

        if (pErrorBlob.get() && pErrorBlob->GetBufferPointer())
            LOG_ERROR(
                "Root signature error: {}",
                eastl::string_view((const char *)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize()));

        ID3D12RootSignature *pRootSig = nullptr;
        m_pDevice->CreateRootSignature(
            0, pRootSigBlob->GetBufferPointer(), pRootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));

        return pRootSig;
    }

    Shader *D3D12API::CreateShader(ShaderStage stage, BufferReadStream &buf)
    {
        ZoneScoped;

        D3D12Shader *pShader = AllocTypeInherit<Shader, D3D12Shader>();

        pShader->m_Type = stage;

        ShaderCompileDesc compileDesc = {};
        compileDesc.m_Type = stage;
        compileDesc.m_Flags = LR_SHADER_FLAG_MATRIX_ROW_MAJOR | LR_SHADER_FLAG_ALL_RESOURCES_BOUND;

#if _DEBUG
        compileDesc.m_Flags |=
            LR_SHADER_FLAG_WARNINGS_ARE_ERRORS | LR_SHADER_FLAG_SKIP_OPTIMIZATION | LR_SHADER_FLAG_USE_DEBUG;
#endif

        pShader->m_pHandle = ShaderCompiler::CompileFromBuffer(&compileDesc, buf);

        return pShader;
    }

    Shader *D3D12API::CreateShader(ShaderStage stage, eastl::string_view path)
    {
        ZoneScoped;

        FileStream fs(path, false);
        void *pData = fs.ReadAll<void>();
        fs.Close();

        BufferReadStream buf(pData, fs.Size());
        Shader *pShader = CreateShader(stage, buf);

        free(pData);

        return pShader;
    }

    void D3D12API::DeleteShader(Shader *pShader)
    {
        ZoneScoped;

        API_VAR(Shader, pShader);

        FreeType(pShaderDX);
    }

    Pipeline *D3D12API::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        D3D12Pipeline *pPipeline = AllocTypeInherit<Pipeline, D3D12Pipeline>();
        D3D12_GRAPHICS_PIPELINE_STATE_DESC createInfo = {};

        constexpr D3D12_BLEND kBlendFactorLUT[] = {
            D3D12_BLEND_ZERO,              // Zero
            D3D12_BLEND_ONE,               // One
            D3D12_BLEND_SRC_COLOR,         // SrcColor
            D3D12_BLEND_INV_SRC_COLOR,     // InvSrcColor
            D3D12_BLEND_SRC_ALPHA,         // SrcAlpha
            D3D12_BLEND_INV_SRC_ALPHA,     // InvSrcAlpha
            D3D12_BLEND_DEST_ALPHA,        // DestAlpha
            D3D12_BLEND_INV_DEST_ALPHA,    // InvDestAlpha
            D3D12_BLEND_DEST_COLOR,        // DestColor
            D3D12_BLEND_INV_DEST_COLOR,    // InvDestColor
            D3D12_BLEND_SRC_ALPHA_SAT,     // SrcAlphaSat
            D3D12_BLEND_BLEND_FACTOR,      // ConstantColor
            D3D12_BLEND_INV_BLEND_FACTOR,  // InvConstantColor
            D3D12_BLEND_SRC1_COLOR,        // Src1Color
            D3D12_BLEND_INV_SRC1_COLOR,    // InvSrc1Color
            D3D12_BLEND_SRC1_ALPHA,        // Src1Alpha
            D3D12_BLEND_INV_SRC1_ALPHA,    // InvSrc1Alpha
        };

        /// PIPELINE LAYOUT ----------------------------------------------------------

        PipelineLayoutSerializeDesc layoutDesc = {};
        layoutDesc.m_DescriptorSetCount = pBuildInfo->m_DescriptorSetCount;
        layoutDesc.m_ppDescriptorSets = pBuildInfo->m_ppDescriptorSets;
        layoutDesc.m_PushConstantCount = pBuildInfo->m_PushConstantCount;
        layoutDesc.m_pPushConstants = pBuildInfo->m_pPushConstants;

        ID3D12RootSignature *pLayout = SerializeRootSignature(&layoutDesc, pPipeline);
        pPipeline->m_pLayout = pLayout;
        createInfo.pRootSignature = pLayout;

        /// BOUND RENDER TARGETS -----------------------------------------------------

        createInfo.NumRenderTargets = pBuildInfo->m_RenderTargetCount;

        for (u32 i = 0; i < pBuildInfo->m_RenderTargetCount; i++)
        {
            createInfo.RTVFormats[i] = ToDXFormat(pBuildInfo->m_pRenderTargets[i].m_Format);
        }

        /// BOUND SHADER STAGES ------------------------------------------------------

        for (u32 i = 0; i < pBuildInfo->m_ShaderCount; i++)
        {
            Shader *pShader = pBuildInfo->m_ppShaders[i];
            API_VAR(D3D12Shader, pShader);

            D3D12_SHADER_BYTECODE *pTargetHandle = nullptr;

            switch (pShader->m_Type)
            {
                case LR_SHADER_STAGE_VERTEX: pTargetHandle = &createInfo.VS; break;
                case LR_SHADER_STAGE_PIXEL: pTargetHandle = &createInfo.PS; break;
                case LR_SHADER_STAGE_HULL: pTargetHandle = &createInfo.HS; break;
                case LR_SHADER_STAGE_DOMAIN: pTargetHandle = &createInfo.DS; break;

                default: assert(!"Shader type not implemented"); break;
            }

            IDxcBlob *pCode = pShaderDX->m_pHandle;

            pTargetHandle->pShaderBytecode = pCode->GetBufferPointer();
            pTargetHandle->BytecodeLength = pCode->GetBufferSize();
        }

        /// INPUT LAYOUT  ------------------------------------------------------------

        InputLayout *pInputLayout = pBuildInfo->m_pInputLayout;
        D3D12_INPUT_ELEMENT_DESC pInputElements[LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE] = {};

        createInfo.InputLayout.NumElements = pInputLayout->m_Count;
        createInfo.InputLayout.pInputElementDescs = pInputElements;

        for (u32 i = 0; i < pInputLayout->m_Count; i++)
        {
            VertexAttrib &element = pInputLayout->m_Elements[i];
            D3D12_INPUT_ELEMENT_DESC &attribDesc = pInputElements[i];

            attribDesc.SemanticName = element.m_Name.data();
            attribDesc.SemanticIndex = 0;
            attribDesc.Format = D3D12API::ToDXFormat(element.m_Type);
            attribDesc.InputSlot = 0;
            attribDesc.AlignedByteOffset = element.m_Offset;
            attribDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            attribDesc.InstanceDataStepRate = 0;
        }

        /// INPUT ASSEMBLY -----------------------------------------------------------

        createInfo.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        /// RASTERIZER ---------------------------------------------------------------

        D3D12_RASTERIZER_DESC &rasterizerInfo = createInfo.RasterizerState;
        rasterizerInfo.DepthClipEnable = pBuildInfo->m_EnableDepthClamp;
        rasterizerInfo.CullMode = ToDXCullMode(pBuildInfo->m_SetCullMode);
        rasterizerInfo.FillMode =
            pBuildInfo->m_SetFillMode == LR_FILL_MODE_FILL ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
        rasterizerInfo.FrontCounterClockwise = !pBuildInfo->m_FrontFaceCCW;
        rasterizerInfo.DepthBias = pBuildInfo->m_EnableDepthBias;
        rasterizerInfo.DepthBias = pBuildInfo->m_DepthBiasFactor;
        rasterizerInfo.DepthBiasClamp = pBuildInfo->m_DepthBiasClamp;
        rasterizerInfo.SlopeScaledDepthBias = pBuildInfo->m_DepthSlopeFactor;

        /// MULTISAMPLE --------------------------------------------------------------

        createInfo.SampleMask = 1 << (pBuildInfo->m_MultiSampleBitCount - 1);
        createInfo.BlendState.AlphaToCoverageEnable = pBuildInfo->m_EnableAlphaToCoverage;
        createInfo.SampleDesc.Count = pBuildInfo->m_MultiSampleBitCount;
        createInfo.SampleDesc.Quality = pBuildInfo->m_MultiSampleBitCount - 1;

        /// DEPTH STENCIL ------------------------------------------------------------

        D3D12_DEPTH_STENCIL_DESC &depthStencilInfo = createInfo.DepthStencilState;
        depthStencilInfo.DepthEnable = pBuildInfo->m_EnableDepthTest;
        depthStencilInfo.DepthWriteMask = (D3D12_DEPTH_WRITE_MASK)pBuildInfo->m_EnableDepthWrite;
        depthStencilInfo.DepthFunc = (D3D12_COMPARISON_FUNC)pBuildInfo->m_DepthCompareOp;
        depthStencilInfo.StencilWriteMask = pBuildInfo->m_EnableStencilTest;

        depthStencilInfo.FrontFace.StencilFunc =
            (D3D12_COMPARISON_FUNC)pBuildInfo->m_StencilFrontFaceOp.m_CompareFunc;
        depthStencilInfo.FrontFace.StencilDepthFailOp =
            (D3D12_STENCIL_OP)pBuildInfo->m_StencilFrontFaceOp.m_DepthFail;
        depthStencilInfo.FrontFace.StencilFailOp = (D3D12_STENCIL_OP)pBuildInfo->m_StencilFrontFaceOp.m_Fail;
        depthStencilInfo.FrontFace.StencilPassOp = (D3D12_STENCIL_OP)pBuildInfo->m_StencilFrontFaceOp.m_Pass;

        depthStencilInfo.BackFace.StencilFunc =
            (D3D12_COMPARISON_FUNC)pBuildInfo->m_StencilBackFaceOp.m_CompareFunc;
        depthStencilInfo.BackFace.StencilDepthFailOp =
            (D3D12_STENCIL_OP)pBuildInfo->m_StencilBackFaceOp.m_DepthFail;
        depthStencilInfo.BackFace.StencilFailOp = (D3D12_STENCIL_OP)pBuildInfo->m_StencilBackFaceOp.m_Fail;
        depthStencilInfo.BackFace.StencilPassOp = (D3D12_STENCIL_OP)pBuildInfo->m_StencilBackFaceOp.m_Pass;

        /// COLOR BLEND --------------------------------------------------------------

        D3D12_BLEND_DESC &colorBlendInfo = createInfo.BlendState;

        colorBlendInfo.IndependentBlendEnable = true;

        for (u32 i = 0; i < pBuildInfo->m_RenderTargetCount; i++)
        {
            PipelineAttachment &attachment = pBuildInfo->m_pRenderTargets[i];
            D3D12_RENDER_TARGET_BLEND_DESC &renderTarget = colorBlendInfo.RenderTarget[i];

            renderTarget.BlendEnable = attachment.m_BlendEnable;
            renderTarget.RenderTargetWriteMask = attachment.m_WriteMask;
            renderTarget.SrcBlend = kBlendFactorLUT[(u32)attachment.m_SrcBlend];
            renderTarget.DestBlend = kBlendFactorLUT[(u32)attachment.m_DstBlend];
            renderTarget.SrcBlendAlpha = kBlendFactorLUT[(u32)attachment.m_SrcBlendAlpha];
            renderTarget.DestBlendAlpha = kBlendFactorLUT[(u32)attachment.m_DstBlendAlpha];
            renderTarget.BlendOp = (D3D12_BLEND_OP)((u32)attachment.m_ColorBlendOp + 1);
            renderTarget.BlendOpAlpha = (D3D12_BLEND_OP)((u32)attachment.m_AlphaBlendOp + 1);
        }

        /// GRAPHICS PIPELINE --------------------------------------------------------

        m_pDevice->CreateGraphicsPipelineState(&createInfo, IID_PPV_ARGS(&pPipeline->m_pHandle));

        return pPipeline;
    }

    Pipeline *D3D12API::CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        D3D12Pipeline *pPipeline = AllocTypeInherit<Pipeline, D3D12Pipeline>();
        D3D12_COMPUTE_PIPELINE_STATE_DESC createInfo = {};

        PipelineLayoutSerializeDesc layoutDesc = {};
        layoutDesc.m_DescriptorSetCount = pBuildInfo->m_DescriptorSetCount;
        layoutDesc.m_ppDescriptorSets = pBuildInfo->m_ppDescriptorSets;
        layoutDesc.m_PushConstantCount = pBuildInfo->m_PushConstantCount;
        layoutDesc.m_pPushConstants = pBuildInfo->m_pPushConstants;

        ID3D12RootSignature *pLayout = SerializeRootSignature(&layoutDesc, pPipeline);
        createInfo.pRootSignature = pLayout;
        pPipeline->m_pLayout = pLayout;

        /// BOUND SHADER STAGES ------------------------------------------------------

        IDxcBlob *pCode = ((D3D12Shader *)pBuildInfo->m_pShader)->m_pHandle;

        createInfo.CS.pShaderBytecode = pCode->GetBufferPointer();
        createInfo.CS.BytecodeLength = pCode->GetBufferSize();

        /// COMPUTE PIPELINE --------------------------------------------------------=

        m_pDevice->CreateComputePipelineState(&createInfo, IID_PPV_ARGS(&pPipeline->m_pHandle));

        return pPipeline;
    }

    void D3D12API::CreateSwapChain(
        IDXGISwapChain1 *&pHandle,
        void *pWindowHandle,
        DXGI_SWAP_CHAIN_DESC1 &desc,
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFSD)
    {
        ZoneScoped;

        m_pFactory->CreateSwapChainForHwnd(
            m_pDirectQueue->m_pHandle, (HWND)pWindowHandle, &desc, pFSD, nullptr, &pHandle);
    }

    void D3D12API::ResizeSwapChain(u32 width, u32 height)
    {
        ZoneScoped;

        m_pDirectQueue->WaitIdle();

        m_SwapChain.m_Width = width;
        m_SwapChain.m_Height = height;

        m_SwapChain.DeleteBuffers(this);
        m_SwapChain.ResizeBuffers();
        m_SwapChain.CreateBackBuffers(this);
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

    DescriptorSet *D3D12API::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        D3D12DescriptorSet *pDescriptorSet = AllocTypeInherit<DescriptorSet, D3D12DescriptorSet>();
        pDescriptorSet->m_BindingCount = pDesc->m_BindingCount;

        D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc = {};
        D3D12_STATIC_SAMPLER_DESC pSamplers[LR_MAX_STATIC_SAMPLERS_PER_PIPELINE] = {};

        u32 samplerCount = 0;
        u32 bindingCount = 0;
        D3D12_ROOT_PARAMETER pBindings[LR_MAX_DESCRIPTORS_PER_LAYOUT] = {};

        for (u32 i = 0; i < pDesc->m_BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->m_pBindings[i];
            pDescriptorSet->m_pBindingTypes[i] = element.m_Type;

            D3D12_ROOT_PARAMETER &binding = pBindings[bindingCount];

            binding.ParameterType = ToDXDescriptorType(element.m_Type);
            binding.ShaderVisibility = ToDXShaderType(element.m_TargetShader);
            binding.Descriptor.RegisterSpace = 0;
            binding.Descriptor.ShaderRegister = element.m_BindingID;

            bindingCount++;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        rootSignatureDesc.Desc_1_0.NumParameters = bindingCount;
        rootSignatureDesc.Desc_1_0.pParameters = pBindings;
        rootSignatureDesc.Desc_1_0.NumStaticSamplers = samplerCount;
        rootSignatureDesc.Desc_1_0.pStaticSamplers = pSamplers;

        ls::scoped_comptr<ID3DBlob> pRootSigBlob;
        ls::scoped_comptr<ID3DBlob> pErrorBlob;
        D3D12SerializeVersionedRootSignature(
            &rootSignatureDesc, pRootSigBlob.get_address(), pErrorBlob.get_address());

        if (pErrorBlob.get() && pErrorBlob->GetBufferPointer())
            LOG_ERROR(
                "Root signature error: {}",
                eastl::string_view((const char *)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize()));

        m_pDevice->CreateRootSignature(
            0,
            pRootSigBlob->GetBufferPointer(),
            pRootSigBlob->GetBufferSize(),
            IID_PPV_ARGS(&pDescriptorSet->m_pHandle));

        return pDescriptorSet;
    }

    void D3D12API::DeleteDescriptorSet(DescriptorSet *pSet)
    {
    }

    void D3D12API::UpdateDescriptorData(DescriptorSet *pSet, DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        API_VAR(D3D12DescriptorSet, pSet);

        for (u32 i = 0; i < pSet->m_BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->m_pBindings[i];

            Sampler *pSampler = element.m_pSampler;
            Buffer *pBuffer = element.m_pBuffer;
            Image *pImage = element.m_pImage;

            API_VAR(D3D12Sampler, pSampler);
            API_VAR(D3D12Buffer, pBuffer);
            API_VAR(D3D12Image, pImage);

            D3D12_GPU_DESCRIPTOR_HANDLE &gpuAddress = pSetDX->m_pDescriptorHandles[i];

            switch (element.m_Type)
            {
                case LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER: gpuAddress = pBufferDX->m_ViewGPU; break;
                case LR_DESCRIPTOR_TYPE_SHADER_RESOURCE: gpuAddress = pImageDX->m_ShaderViewGPU; break;
                case LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE:
                    gpuAddress = pImageDX->m_UnorderedAccessViewGPU;
                    break;
                case LR_DESCRIPTOR_TYPE_SAMPLER: gpuAddress = pSamplerDX->m_HandleGPU;
                default: break;
            }
        }
    }

    void D3D12API::CreateDescriptorPool(const std::initializer_list<D3D12DescriptorBindingDesc> &bindings)
    {
        ZoneScoped;

        for (auto &element : bindings)
        {
            D3D12DescriptorHeap &heap = m_pDescriptorHeaps[element.m_HeapType];
            ID3D12DescriptorHeap *pHandle = heap.m_pHandle;

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = element.m_HeapType;
            heapDesc.NumDescriptors = element.m_Count;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (element.m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV
                || element.m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHandle));

            heap.m_pHandle = pHandle;
            heap.m_IncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(element.m_HeapType);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12API::AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ZoneScoped;

        D3D12DescriptorHeap &heap = m_pDescriptorHeaps[type];

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.m_pHandle->GetCPUDescriptorHandleForHeapStart();

        if (index == -1)
        {
            handle.ptr += heap.m_IncrementSize * heap.m_CPUOffset;
            heap.m_CPUOffset++;
        }
        else
        {
            handle.ptr += heap.m_IncrementSize * index;
        }

        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12API::AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ZoneScoped;

        D3D12DescriptorHeap &heap = m_pDescriptorHeaps[type];

        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.m_pHandle->GetGPUDescriptorHandleForHeapStart();

        if (index == -1)
        {
            handle.ptr += heap.m_IncrementSize * heap.m_GPUOffset;
            heap.m_GPUOffset++;
        }
        else
        {
            handle.ptr += heap.m_IncrementSize * index;
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

    Buffer *D3D12API::CreateBuffer(BufferDesc *pDesc)
    {
        ZoneScoped;

        D3D12Buffer *pBuffer = AllocTypeInherit<Buffer, D3D12Buffer>();
        pBuffer->m_UsageFlags = pDesc->m_UsageFlags;
        pBuffer->m_TargetAllocator = pDesc->m_TargetAllocator;
        pBuffer->m_Stride = pDesc->m_Stride;
        pBuffer->m_DataLen = pDesc->m_DataLen;

        /// ---------------------------------------------- ///

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        resourceDesc.Width = pBuffer->m_DataLen;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        /// ---------------------------------------------- ///

        SetAllocator(pBuffer, resourceDesc, pBuffer->m_TargetAllocator);

        m_pDevice->CreatePlacedResource(
            pBuffer->m_pMemoryHandle,
            0,  // TODO: SUBRESOURCE ALLOCATION
            &resourceDesc,
            ToDXBufferUsage(pBuffer->m_UsageFlags),
            nullptr,
            IID_PPV_ARGS(&pBuffer->m_pHandle));

        pBuffer->m_VirtualAddress = pBuffer->m_pHandle->GetGPUVirtualAddress();

        CreateBufferView(pBuffer);

        return pBuffer;
    }

    void D3D12API::DeleteBuffer(Buffer *pHandle)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pHandle);

        if (pHandleDX->m_pMemoryHandle)
        {
            if (pHandleDX->m_TargetAllocator == LR_API_ALLOCATOR_BUFFER_TLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pHandleDX->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MABufferTLSF.Allocator.Free(pBlock);
            }
            else if (pHandleDX->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
            {
                pHandleDX->m_pMemoryHandle->Release();
            }
        }

        if (pHandleDX->m_pHandle)
            pHandleDX->m_pHandle->Release();
    }

    void D3D12API::CreateBufferView(Buffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        if (pBuffer->m_UsageFlags & LR_RESOURCE_USAGE_CONSTANT_BUFFER)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
            viewDesc.BufferLocation = pBufferDX->m_VirtualAddress;
            viewDesc.SizeInBytes = pBufferDX->m_DataLen;

            pBufferDX->m_ViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            pBufferDX->m_ViewGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_pDevice->CreateConstantBufferView(&viewDesc, pBufferDX->m_ViewCPU);
        }
    }

    void D3D12API::MapMemory(Buffer *pBuffer, void *&pData)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        pBufferDX->m_pHandle->Map(0, nullptr, &pData);
    }

    void D3D12API::UnmapMemory(Buffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        pBufferDX->m_pHandle->Unmap(0, nullptr);
    }

    Image *D3D12API::CreateImage(ImageDesc *pDesc)
    {
        ZoneScoped;

        D3D12Image *pImage = AllocTypeInherit<Image, D3D12Image>();
        pImage->m_UsageFlags = pDesc->m_UsageFlags;
        pImage->m_Format = pDesc->m_Format;
        pImage->m_TargetAllocator = pDesc->m_TargetAllocator;
        pImage->m_Width = pDesc->m_Width;
        pImage->m_Height = pDesc->m_Height;
        pImage->m_ArraySize = pDesc->m_ArraySize;
        pImage->m_MipMapLevels = pDesc->m_MipMapLevels;
        pImage->m_DataLen = pDesc->m_DataLen;

        /// ---------------------------------------------- ///

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        if (pImage->m_UsageFlags & LR_RESOURCE_USAGE_RENDER_TARGET)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (pImage->m_UsageFlags & LR_RESOURCE_USAGE_DEPTH_STENCIL)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (pImage->m_UsageFlags & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = pImage->m_Width;
        resourceDesc.Height = pImage->m_Height;
        resourceDesc.DepthOrArraySize = pImage->m_ArraySize;
        resourceDesc.MipLevels = pImage->m_MipMapLevels;
        resourceDesc.Format = ToDXFormat(pImage->m_Format);
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = flags;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;

        /// ---------------------------------------------- ///

        SetAllocator(pImage, resourceDesc, pImage->m_TargetAllocator);

        D3D12_RESOURCE_STATES initialState = ToDXImageUsage(pImage->m_UsageFlags);
        initialState &= ~D3D12_RESOURCE_STATE_COPY_DEST;
        initialState &= ~D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

        m_pDevice->CreatePlacedResource(
            pImage->m_pMemoryHandle,
            0,  // TODO: Suballocations
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&pImage->m_pHandle));

        CreateImageView(pImage);

        return pImage;
    }

    void D3D12API::DeleteImage(Image *pImage)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        if (pImageDX->m_pMemoryHandle)
        {
            if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_IMAGE_TLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pImage->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MAImageTLSF.Allocator.Free(pBlock);
            }
            else if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
            {
                pImageDX->m_pMemoryHandle->Release();
            }
        }

        if (pImageDX->m_pHandle)
            pImageDX->m_pHandle->Release();
    }

    void D3D12API::CreateImageView(Image *pImage)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        if (pImageDX->m_UsageFlags & LR_RESOURCE_USAGE_SHADER_RESOURCE)
        {
            pImageDX->m_ShaderViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            pImageDX->m_ShaderViewGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImageDX->m_Format);
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            viewDesc.Texture2D.MostDetailedMip = 0;
            viewDesc.Texture2D.MipLevels = pImage->m_MipMapLevels;
            viewDesc.Texture2D.PlaneSlice = 0;
            viewDesc.Texture2D.ResourceMinLODClamp = 0;

            m_pDevice->CreateShaderResourceView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_ShaderViewCPU);
        }

        if (pImageDX->m_UsageFlags & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
        {
            pImageDX->m_UnorderedAccessViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            pImageDX->m_UnorderedAccessViewGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImageDX->m_Format);
            viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice = 0;
            viewDesc.Texture2D.PlaneSlice = 0;

            m_pDevice->CreateUnorderedAccessView(
                pImageDX->m_pHandle, nullptr, &viewDesc, pImageDX->m_UnorderedAccessViewCPU);
        }

        if (pImageDX->m_UsageFlags & LR_RESOURCE_USAGE_RENDER_TARGET)
        {
            pImageDX->m_RenderTargetViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImage->m_Format);
            viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice = 0;

            m_pDevice->CreateRenderTargetView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_RenderTargetViewCPU);
        }

        if (pImageDX->m_UsageFlags & LR_RESOURCE_USAGE_DEPTH_STENCIL)
        {
            pImageDX->m_DepthStencilViewCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

            D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
            viewDesc.Format = ToDXFormat(pImage->m_Format);
            viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipSlice = 0;

            m_pDevice->CreateDepthStencilView(pImageDX->m_pHandle, &viewDesc, pImageDX->m_DepthStencilViewCPU);
        }
    }

    Sampler *D3D12API::CreateSampler(SamplerDesc *pDesc)
    {
        ZoneScoped;

        D3D12Sampler *pSampler = AllocTypeInherit<Sampler, D3D12Sampler>();

        constexpr static D3D12_TEXTURE_ADDRESS_MODE kAddresModeLUT[] = {
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        };

        u32 minFilterVal = (u32)pDesc->m_MinFilter;
        u32 magFilterVal = (u32)pDesc->m_MagFilter;
        u32 mipFilterVal = (u32)pDesc->m_MipFilter;

        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter =
            (D3D12_FILTER)(mipFilterVal | (magFilterVal << 2) | (minFilterVal << 4) | (pDesc->m_UseAnisotropy << 6));

        samplerDesc.AddressU = kAddresModeLUT[(u32)pDesc->m_AddressU];
        samplerDesc.AddressV = kAddresModeLUT[(u32)pDesc->m_AddressV];
        samplerDesc.AddressW = kAddresModeLUT[(u32)pDesc->m_AddressW];

        samplerDesc.MaxAnisotropy = pDesc->m_MaxAnisotropy;

        samplerDesc.MipLODBias = pDesc->m_MipLODBias;
        samplerDesc.MaxLOD = pDesc->m_MaxLOD;
        samplerDesc.MinLOD = pDesc->m_MinLOD;

        samplerDesc.ComparisonFunc = (D3D12_COMPARISON_FUNC)pDesc->m_CompareOp;

        samplerDesc.BorderColor[0] = 0.0;
        samplerDesc.BorderColor[1] = 0.0;
        samplerDesc.BorderColor[2] = 0.0;
        samplerDesc.BorderColor[3] = 0.0;

        pSampler->m_HandleCPU = AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        pSampler->m_HandleGPU = AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        m_pDevice->CreateSampler(&samplerDesc, pSampler->m_HandleCPU);

        return pSampler;
    }

    void D3D12API::SetAllocator(
        D3D12Buffer *pBuffer, D3D12_RESOURCE_DESC &resourceDesc, APIAllocatorType targetAllocator)
    {
        ZoneScoped;

        D3D12_RESOURCE_ALLOCATION_INFO memoryInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);
        pBuffer->m_DeviceDataLen = memoryInfo.SizeInBytes;

        switch (targetAllocator)
        {
            case LR_API_ALLOCATOR_NONE:
            {
                pBuffer->m_DataOffset = 0;
                pBuffer->m_pMemoryHandle = CreateHeap(
                    pBuffer->m_DeviceDataLen,
                    pBuffer->m_UsageFlags & LR_RESOURCE_USAGE_HOST_VISIBLE,
                    D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);

                break;
            }

            case LR_API_ALLOCATOR_DESCRIPTOR:
            {
                pBuffer->m_DataOffset =
                    m_MADescriptor.Allocator.Allocate(pBuffer->m_DeviceDataLen, pBuffer->m_Alignment);
                pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

                break;
            }

            case LR_API_ALLOCATOR_BUFFER_LINEAR:
            {
                pBuffer->m_DataOffset =
                    m_MABufferLinear.Allocator.Allocate(pBuffer->m_DeviceDataLen, pBuffer->m_Alignment);
                pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

                break;
            }

            case LR_API_ALLOCATOR_BUFFER_TLSF:
            {
                Memory::TLSFBlock *pBlock =
                    m_MABufferTLSF.Allocator.Allocate(pBuffer->m_DeviceDataLen, pBuffer->m_Alignment);

                pBuffer->m_pAllocatorData = pBlock;
                pBuffer->m_DataOffset = pBlock->m_Offset;

                pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

                break;
            }

            default: LOG_CRITICAL("Trying to allocate D3D12Buffer resource from invalid allocator."); break;
        }
    }

    void D3D12API::SetAllocator(
        D3D12Image *pImage, D3D12_RESOURCE_DESC &resourceDesc, APIAllocatorType targetAllocator)
    {
        ZoneScoped;

        D3D12_RESOURCE_ALLOCATION_INFO memoryInfo = m_pDevice->GetResourceAllocationInfo(0, 1, &resourceDesc);
        pImage->m_DeviceDataLen = memoryInfo.SizeInBytes;

        switch (targetAllocator)
        {
            case LR_API_ALLOCATOR_NONE:
            {
                pImage->m_DataOffset = 0;
                pImage->m_pMemoryHandle =
                    CreateHeap(memoryInfo.SizeInBytes, false, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);

                break;
            }

            case LR_API_ALLOCATOR_IMAGE_TLSF:
            {
                Memory::TLSFBlock *pBlock =
                    m_MAImageTLSF.Allocator.Allocate(pImage->m_DeviceDataLen, memoryInfo.Alignment);

                pImage->m_pAllocatorData = pBlock;
                pImage->m_DataOffset = pBlock->m_Offset;

                pImage->m_pMemoryHandle = m_MAImageTLSF.pHeap;

                break;
            }

            default: LOG_CRITICAL("Trying to allocate D3D12Image resource from invalid allocator."); break;
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

                CommandList *pList = commandPool.m_DirectLists[index];
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
        D3D12_ROOT_PARAMETER_TYPE_SRV,              // LR_DESCRIPTOR_TYPE_SAMPLER
        D3D12_ROOT_PARAMETER_TYPE_SRV,              // LR_DESCRIPTOR_TYPE_SHADER_RESOURCE
        D3D12_ROOT_PARAMETER_TYPE_CBV,              // LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER
        D3D12_ROOT_PARAMETER_TYPE_UAV,              // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE
        D3D12_ROOT_PARAMETER_TYPE_UAV,              // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,  // LR_DESCRIPTOR_TYPE_PUSH_CONSTANT
    };

    constexpr D3D12_DESCRIPTOR_RANGE_TYPE kDescriptorRangeLUT[] = {
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,  // LR_DESCRIPTOR_TYPE_SAMPLER
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,      // LR_DESCRIPTOR_TYPE_SHADER_RESOURCE
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV,      // LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,      // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV,      // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER
    };

    constexpr D3D12_COMMAND_LIST_TYPE kCommandListType[] = {
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        D3D12_COMMAND_LIST_TYPE_COPY,
    };

    DXGI_FORMAT D3D12API::ToDXFormat(ImageFormat format)
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

    D3D12_DESCRIPTOR_RANGE_TYPE D3D12API::ToDXDescriptorRangeType(DescriptorType type)
    {
        return kDescriptorRangeLUT[(u32)type];
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXImageUsage(ResourceUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage == LR_RESOURCE_USAGE_UNKNOWN || usage == LR_RESOURCE_USAGE_PRESENT)
            return D3D12_RESOURCE_STATE_COMMON;

        if (usage & LR_RESOURCE_USAGE_RENDER_TARGET)
            v |= D3D12_RESOURCE_STATE_RENDER_TARGET;

        if (usage & LR_RESOURCE_USAGE_DEPTH_STENCIL)
            v |= D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ;

        if (usage & LR_RESOURCE_USAGE_SHADER_RESOURCE)
            v |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_SRC)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_DST)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        if (usage & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
            v |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        return v;
    }

    D3D12_RESOURCE_STATES D3D12API::ToDXBufferUsage(ResourceUsage usage)
    {
        D3D12_RESOURCE_STATES v = {};

        if (usage & LR_RESOURCE_USAGE_VERTEX_BUFFER)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

        if (usage & LR_RESOURCE_USAGE_INDEX_BUFFER)
            v |= D3D12_RESOURCE_STATE_INDEX_BUFFER;

        if (usage & LR_RESOURCE_USAGE_CONSTANT_BUFFER)
            v |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_SRC)
            v |= D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_DST)
            v |= D3D12_RESOURCE_STATE_COPY_DEST;

        if (usage & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
            v |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        return v;
    }

    D3D12_SHADER_VISIBILITY D3D12API::ToDXShaderType(ShaderStage type)
    {
        u32 v = {};

        if (type & LR_SHADER_STAGE_VERTEX)
            v |= D3D12_SHADER_VISIBILITY_VERTEX;

        if (type & LR_SHADER_STAGE_PIXEL)
            v |= D3D12_SHADER_VISIBILITY_PIXEL;

        if (type & LR_SHADER_STAGE_COMPUTE)
            v |= D3D12_SHADER_VISIBILITY_ALL;

        if (type & LR_SHADER_STAGE_HULL)
            v |= D3D12_SHADER_VISIBILITY_HULL;

        if (type & LR_SHADER_STAGE_DOMAIN)
            v |= D3D12_SHADER_VISIBILITY_DOMAIN;

        return (D3D12_SHADER_VISIBILITY)v;
    }

    D3D12_COMMAND_LIST_TYPE D3D12API::ToDXCommandListType(CommandListType type)
    {
        return kCommandListType[(u32)type];
    }

}  // namespace lr::Graphics