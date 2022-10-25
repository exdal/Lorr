#include "D3D12API.hh"

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

        return true;
    }

    void D3D12API::InitCommandLists()
    {
        ZoneScoped;
    }

    void D3D12API::InitAllocators()
    {
        ZoneScoped;
    }

    void D3D12API::CreateCommandQueue(D3D12CommandQueue *pQueue, CommandListType type)
    {
        ZoneScoped;

        ID3D12CommandQueue *&pHandle = pQueue->m_pHandle;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.NodeMask = 0;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;  // TODO: Multiple Queues
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pHandle));
    }

    void D3D12API::CreateCommandAllocator(D3D12CommandAllocator *pAllocator, CommandListType type)
    {
        ZoneScoped;

        ID3D12CommandAllocator *&pHandle = pAllocator->m_pHandle;

        m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pHandle));
    }

    void D3D12API::CreateCommandList(D3D12CommandList *pList, CommandListType type)
    {
        ZoneScoped;

        ID3D12GraphicsCommandList4 *&pHandle = pList->m_pHandle;
        ID3D12CommandAllocator *&pAllocator = pList->m_Allocator.m_pHandle;

        CreateCommandAllocator(&pList->m_Allocator, type);

        m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, IID_PPV_ARGS(&pHandle));

        /// ---------------------------------------------- ///

        pList->m_Type = type;
    }

    D3D12CommandList *D3D12API::GetCommandList()
    {
        ZoneScoped;

        return nullptr;
    }

    void D3D12API::BeginCommandList(D3D12CommandList *pList)
    {
        ZoneScoped;

        pList->Reset();
    }

    void D3D12API::EndCommandList(D3D12CommandList *pList)
    {
        ZoneScoped;

        pList->Close();
    }

    void D3D12API::ResetCommandAllocator(D3D12CommandAllocator *pAllocator)
    {
        ZoneScoped;

        pAllocator->Reset();
    }

    void D3D12API::ExecuteCommandList(D3D12CommandList *pList, bool waitForFence)
    {
        ZoneScoped;
    }

}  // namespace lr::Graphics