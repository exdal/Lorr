#include "D3D12CommandQueue.hh"

namespace lr::Graphics
{
    void D3D12CommandQueue::ExecuteCommandList(D3D12CommandList *pList)
    {
        ZoneScoped;

        u64 completed = pList->m_pFence->GetCompletedValue();
        u64 desired = pList->m_DesiredFenceValue.add_fetch(1, eastl::memory_order_acq_rel);

        ID3D12CommandList *const ppList[] = { pList->m_pHandle };

        m_pHandle->ExecuteCommandLists(1, ppList);
        m_pHandle->Signal(pList->m_pFence, desired);
    }

}  // namespace lr::Graphics