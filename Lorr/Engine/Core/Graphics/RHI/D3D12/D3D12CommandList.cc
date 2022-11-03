#include "D3D12CommandList.hh"

#include "D3D12API.hh"
#include "D3D12Resource.hh"

#define API_VAR(type, name) type *name##DX = (type *)name

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

    void D3D12CommandList::BarrierTransition(BaseImage *pImage,
                              ResourceUsage barrierBefore,
                              ShaderStage shaderBefore,
                              ResourceUsage barrierAfter,
                              ShaderStage shaderAfter)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pImageDX->m_pHandle;

        barrier.Transition.StateBefore = D3D12API::ToDXImageLayout(barrierBefore);
        barrier.Transition.StateAfter = D3D12API::ToDXImageLayout(barrierAfter);

        m_pHandle->ResourceBarrier(1, &barrier);
    }

}  // namespace lr::Graphics