#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    void D3D12CommandList::Init(ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_pAllocator = pAllocator;

        m_Type = type;

        m_pHandle->Close();
        m_DesiredFenceValue = 0;
    }

    void D3D12CommandList::Reset(D3D12CommandAllocator *pAllocator)
    {
        ZoneScoped;

        m_pHandle->Reset(pAllocator->pHandle, nullptr);
    }

}  // namespace lr::Graphics