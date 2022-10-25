#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    void D3D12CommandAllocator::Reset()
    {
        ZoneScoped;

        m_pHandle->Reset();
    }

    void D3D12CommandList::Reset()
    {
        ZoneScoped;

        m_Allocator.Reset();
        m_pHandle->Reset(m_Allocator.m_pHandle, nullptr);
    }

    void D3D12CommandList::Close()
    {
        ZoneScoped;

        m_pHandle->Close();
    }

}  // namespace lr::Graphics