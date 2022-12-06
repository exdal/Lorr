#include "D3D12CommandList.hh"

#include "D3D12API.hh"
#include "D3D12Resource.hh"
#include "D3D12Pipeline.hh"

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

    void D3D12CommandList::BeginPass(CommandListBeginDesc *pDesc)
    {
        ZoneScoped;

        static constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE kBeginningAccessLUT[] = {
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,   // Load
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,  // Store
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,      // Clear
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,  // DontCare
        };

        static constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE kEndingAccessLUT[] = {
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // Load
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,   // Store
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // Clear
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // DontCare
        };

        D3D12_RENDER_PASS_RENDER_TARGET_DESC pColorAttachments[APIConfig::kMaxColorAttachmentCount] = {};
        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthAttachment = {};

        for (u32 i = 0; i < pDesc->ColorAttachmentCount; i++)
        {
            CommandListAttachment &attachment = pDesc->pColorAttachments[i];
            D3D12_RENDER_PASS_RENDER_TARGET_DESC &info = pColorAttachments[i];

            BaseImage *pImage = attachment.pHandle;
            API_VAR(D3D12Image, pImage);

            info.cpuDescriptor = pImageDX->m_ShaderViewCPU;
            info.BeginningAccess.Type = kBeginningAccessLUT[(u32)attachment.LoadOp];
            info.EndingAccess.Type = kEndingAccessLUT[(u32)attachment.StoreOp];

            D3D12_CLEAR_VALUE &clearValue = info.BeginningAccess.Clear.ClearValue;
            clearValue.Format = D3D12API::ToDXFormat(pImage->m_Format);

            memcpy(&clearValue.Color, &attachment.ClearVal.RenderTargetColor, sizeof(XMFLOAT4));
        }

        if (pDesc->pDepthAttachment)
        {
            CommandListAttachment &attachment = *pDesc->pDepthAttachment;

            BaseImage *pImage = attachment.pHandle;
            API_VAR(D3D12Image, pImage);

            depthAttachment.cpuDescriptor = pImageDX->m_DepthStencilViewCPU;
            depthAttachment.DepthBeginningAccess.Type = kBeginningAccessLUT[(u32)attachment.LoadOp];
            depthAttachment.DepthEndingAccess.Type = kEndingAccessLUT[(u32)attachment.StoreOp];

            D3D12_CLEAR_VALUE &clearValue = depthAttachment.DepthBeginningAccess.Clear.ClearValue;
            clearValue.Format = D3D12API::ToDXFormat(pImage->m_Format);

            memcpy(&clearValue.Color, &attachment.ClearVal.DepthStencilColor, sizeof(ClearValue::Depth));
        }

        m_pHandle->BeginRenderPass(pDesc->ColorAttachmentCount,
                                   pColorAttachments,
                                   pDesc->pDepthAttachment ? &depthAttachment : nullptr,
                                   D3D12_RENDER_PASS_FLAG_NONE);
    }

    void D3D12CommandList::EndPass()
    {
        ZoneScoped;

        m_pHandle->EndRenderPass();
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

    void D3D12CommandList::BarrierTransition(BaseBuffer *pBuffer,
                                             ResourceUsage barrierBefore,
                                             ShaderStage shaderBefore,
                                             ResourceUsage barrierAfter,
                                             ShaderStage shaderAfter)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pBufferDX->m_pHandle;

        barrier.Transition.StateBefore = D3D12API::ToDXBufferUsage(barrierBefore);
        barrier.Transition.StateAfter = D3D12API::ToDXBufferUsage(barrierAfter);

        m_pHandle->ResourceBarrier(1, &barrier);
    }

    void D3D12CommandList::ClearImage(BaseImage *pImage, ClearValue val)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        m_pHandle->ClearRenderTargetView(pImageDX->m_RenderTargetViewCPU, &val.RenderTargetColor.x, 0, nullptr);
    }

    void D3D12CommandList::SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height)
    {
        ZoneScoped;

        D3D12_VIEWPORT vp;
        vp.TopLeftX = x;
        vp.TopLeftY = y;
        vp.Width = width;
        vp.Height = height;
        vp.MinDepth = 0.0;
        vp.MaxDepth = 1.0;

        m_pHandle->RSSetViewports(1, &vp);
    }

    void D3D12CommandList::SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height)
    {
        ZoneScoped;

        D3D12_RECT rect;
        rect.left = x;
        rect.top = y;
        rect.right = width;
        rect.bottom = height;

        m_pHandle->RSSetScissorRects(1, &rect);
    }

    void D3D12CommandList::SetPrimitiveType(PrimitiveType type)
    {
        ZoneScoped;

        m_pHandle->IASetPrimitiveTopology(D3D12API::ToDXTopology(type));
    }

    void D3D12CommandList::SetVertexBuffer(BaseBuffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_VERTEX_BUFFER_VIEW view = {};
        view.BufferLocation = pBufferDX->m_pHandle->GetGPUVirtualAddress();
        view.SizeInBytes = pBuffer->m_DataSize;
        view.StrideInBytes = pBuffer->m_Stride;

        m_pHandle->IASetVertexBuffers(0, 1, &view);
    }

    void D3D12CommandList::SetIndexBuffer(BaseBuffer *pBuffer, bool type32)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_INDEX_BUFFER_VIEW view = {};
        view.BufferLocation = pBufferDX->m_pHandle->GetGPUVirtualAddress();
        view.SizeInBytes = pBuffer->m_DataSize;
        view.Format = type32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

        m_pHandle->IASetIndexBuffer(&view);
    }

    void D3D12CommandList::CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pSource);
        API_VAR(D3D12Buffer, pDest);

        m_pHandle->CopyBufferRegion(pDestDX->m_pHandle, 0, pSourceDX->m_pHandle, 0, size);
    }

    void D3D12CommandList::CopyBuffer(BaseBuffer *pSource, BaseImage *pDest)
    {
        ZoneScoped;
    }

    void D3D12CommandList::Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        m_pHandle->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void D3D12CommandList::DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        m_pHandle->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void D3D12CommandList::SetPipeline(BasePipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        m_pHandle->SetPipelineState(pPipelineDX->pHandle);
        m_pHandle->SetGraphicsRootSignature(pPipelineDX->pLayout);
    }

    void D3D12CommandList::SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        for (BaseDescriptorSet *pSet : sets)
        {
            API_VAR(D3D12DescriptorSet, pSet);

            m_pHandle->SetGraphicsRootDescriptorTable(idx++, pSetDX->m_pDescriptorHandles[0]);
        }
    }

}  // namespace lr::Graphics