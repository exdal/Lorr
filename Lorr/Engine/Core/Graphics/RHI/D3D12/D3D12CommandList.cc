#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    void D3D12CommandList::Init(ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_pAllocator = pAllocator;

        m_Type = type;
    }

}  // namespace lr::Graphics