#include "D3D12CommandList.hh"

#include "D3D12API.hh"
#include "D3D12Resource.hh"
#include "D3D12Pipeline.hh"

#define API_VAR(type, name) type *name##DX = (type *)name

namespace lr::Graphics
{
    void D3D12CommandList::Init(
        ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type)
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

        m_pHandle->Reset(pAllocator->m_pHandle, nullptr);
    }

    void D3D12CommandList::BeginPass(CommandListBeginDesc *pDesc)
    {
        ZoneScoped;

        static constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE kBeginningAccessLUT[] = {
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,   // Load
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,  // Store
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,      // Clear
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,   // DontCare
        };

        static constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE kEndingAccessLUT[] = {
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // Load
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,   // Store
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // Clear
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,  // DontCare
        };

        D3D12_RENDER_PASS_RENDER_TARGET_DESC pColorAttachments[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthAttachment = {};

        for (u32 i = 0; i < pDesc->m_ColorAttachmentCount; i++)
        {
            CommandListAttachment &attachment = pDesc->m_pColorAttachments[i];
            D3D12_RENDER_PASS_RENDER_TARGET_DESC &info = pColorAttachments[i];

            Image *pImage = attachment.m_pHandle;
            API_VAR(D3D12Image, pImage);

            info.cpuDescriptor = pImageDX->m_RenderTargetViewCPU;
            info.BeginningAccess.Type = kBeginningAccessLUT[(u32)attachment.m_LoadOp];
            info.EndingAccess.Type = kEndingAccessLUT[(u32)attachment.m_StoreOp];

            D3D12_CLEAR_VALUE &clearValue = info.BeginningAccess.Clear.ClearValue;
            clearValue.Format = D3D12API::ToDXFormat(pImage->m_Format);

            memcpy(&clearValue.Color, &attachment.m_ClearVal.m_RenderTargetColor, sizeof(XMFLOAT4));
        }

        if (pDesc->m_pDepthAttachment)
        {
            CommandListAttachment &attachment = *pDesc->m_pDepthAttachment;

            Image *pImage = attachment.m_pHandle;
            API_VAR(D3D12Image, pImage);

            depthAttachment.cpuDescriptor = pImageDX->m_DepthStencilViewCPU;
            depthAttachment.DepthBeginningAccess.Type = kBeginningAccessLUT[(u32)attachment.m_LoadOp];
            depthAttachment.DepthEndingAccess.Type = kEndingAccessLUT[(u32)attachment.m_StoreOp];

            D3D12_CLEAR_VALUE &clearValue = depthAttachment.DepthBeginningAccess.Clear.ClearValue;
            clearValue.Format = D3D12API::ToDXFormat(pImage->m_Format);

            memcpy(&clearValue.Color, &attachment.m_ClearVal.m_DepthStencilColor, sizeof(ClearValue::Depth));
        }

        m_pHandle->BeginRenderPass(
            pDesc->m_ColorAttachmentCount,
            pColorAttachments,
            pDesc->m_pDepthAttachment ? &depthAttachment : nullptr,
            D3D12_RENDER_PASS_FLAG_NONE);
    }

    void D3D12CommandList::EndPass()
    {
        ZoneScoped;

        m_pHandle->EndRenderPass();
    }

    void D3D12CommandList::ClearImage(Image *pImage, ClearValue val)
    {
        ZoneScoped;

        API_VAR(D3D12Image, pImage);

        m_pHandle->ClearRenderTargetView(
            pImageDX->m_RenderTargetViewCPU, &val.m_RenderTargetColor.x, 0, nullptr);
    }

    void D3D12CommandList::SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        if (pBarrier->m_CurrentUsage == pBarrier->m_NextUsage)
            return;

        API_VAR(D3D12Image, pImage);

        u32 setBarrierCount = 0;
        D3D12_RESOURCE_BARRIER pBarriers[3] = {};  // by index: 0, src uav, 1 transition, 2 dst uav

        if (pBarrier->m_CurrentStage == LR_PIPELINE_STAGE_COMPUTE_SHADER)
        {
            D3D12_RESOURCE_BARRIER &barrier = pBarriers[setBarrierCount++];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = pImageDX->m_pHandle;
        }

        D3D12_RESOURCE_BARRIER &barrier = pBarriers[setBarrierCount++];
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pImageDX->m_pHandle;
        barrier.Transition.StateBefore = D3D12API::ToDXImageUsage(pBarrier->m_CurrentUsage);
        barrier.Transition.StateAfter = D3D12API::ToDXImageUsage(pBarrier->m_NextUsage);

        if (pBarrier->m_NextStage == LR_PIPELINE_STAGE_COMPUTE_SHADER)
        {
            D3D12_RESOURCE_BARRIER &barrier = pBarriers[setBarrierCount++];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = pImageDX->m_pHandle;
        }

        m_pHandle->ResourceBarrier(setBarrierCount, pBarriers);
    }

    void D3D12CommandList::SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        if (pBarrier->m_CurrentUsage == pBarrier->m_NextUsage)
            return;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pBufferDX->m_pHandle;

        barrier.Transition.StateBefore = D3D12API::ToDXImageUsage(pBarrier->m_CurrentUsage);
        barrier.Transition.StateAfter = D3D12API::ToDXImageUsage(pBarrier->m_NextUsage);

        m_pHandle->ResourceBarrier(1, &barrier);
    }

    void D3D12CommandList::SetVertexBuffer(Buffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_VERTEX_BUFFER_VIEW view = {};
        view.BufferLocation = pBufferDX->m_pHandle->GetGPUVirtualAddress();
        view.SizeInBytes = pBuffer->m_DataLen;
        view.StrideInBytes = pBuffer->m_Stride;

        m_pHandle->IASetVertexBuffers(0, 1, &view);
    }

    void D3D12CommandList::SetIndexBuffer(Buffer *pBuffer, bool type32)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pBuffer);

        D3D12_INDEX_BUFFER_VIEW view = {};
        view.BufferLocation = pBufferDX->m_pHandle->GetGPUVirtualAddress();
        view.SizeInBytes = pBuffer->m_DataLen;
        view.Format = type32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

        m_pHandle->IASetIndexBuffer(&view);
    }

    void D3D12CommandList::CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pSource);
        API_VAR(D3D12Buffer, pDest);

        m_pHandle->CopyBufferRegion(pDestDX->m_pHandle, 0, pSourceDX->m_pHandle, 0, size);
    }

    void D3D12CommandList::CopyBuffer(Buffer *pSource, Image *pDest)
    {
        ZoneScoped;

        API_VAR(D3D12Buffer, pSource);
        API_VAR(D3D12Image, pDest);

        D3D12_TEXTURE_COPY_LOCATION destLocation = {};
        destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLocation.SubresourceIndex = 0;  // TODO: Mips
        destLocation.pResource = pDestDX->m_pHandle;

        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Footprint.Format = D3D12API::ToDXFormat(pDest->m_Format);
        srcLocation.PlacedFootprint.Footprint.Width = pDest->m_Width;
        srcLocation.PlacedFootprint.Footprint.Height = pDest->m_Height;
        srcLocation.PlacedFootprint.Footprint.Depth = 1;
        srcLocation.PlacedFootprint.Footprint.RowPitch =
            Memory::AlignUp(pDest->m_Width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        srcLocation.pResource = pSourceDX->m_pHandle;

        m_pHandle->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
    }

    void D3D12CommandList::Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        m_pHandle->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void D3D12CommandList::DrawIndexed(
        u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        m_pHandle->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void D3D12CommandList::Dispatch(u32 groupX, u32 groupY, u32 groupZ)
    {
        ZoneScoped;

        m_pHandle->Dispatch(groupX, groupY, groupZ);
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

    void D3D12CommandList::SetGraphicsPipeline(Pipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        m_pHandle->SetPipelineState(pPipelineDX->m_pHandle);
        m_pHandle->SetGraphicsRootSignature(pPipelineDX->m_pLayout);
    }

    void D3D12CommandList::SetComputePipeline(Pipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        m_pHandle->SetPipelineState(pPipelineDX->m_pHandle);
        m_pHandle->SetComputeRootSignature(pPipelineDX->m_pLayout);
    }

    void D3D12CommandList::SetGraphicsDescriptorSets(const std::initializer_list<DescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        for (DescriptorSet *pSet : sets)
        {
            API_VAR(D3D12DescriptorSet, pSet);

            m_pHandle->SetGraphicsRootDescriptorTable(idx++, pSetDX->m_pDescriptorHandles[0]);
        }
    }

    void D3D12CommandList::SetComputeDescriptorSets(const std::initializer_list<DescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        for (DescriptorSet *pSet : sets)
        {
            API_VAR(D3D12DescriptorSet, pSet);

            m_pHandle->SetComputeRootDescriptorTable(idx++, pSetDX->m_pDescriptorHandles[0]);
        }
    }

    void D3D12CommandList::SetGraphicsPushConstants(
        Pipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        m_pHandle->SetGraphicsRoot32BitConstants(
            pPipelineDX->m_pRootConstats[(u32)D3D12API::ToDXShaderType(stage)],
            dataSize / sizeof(u32),
            pData,
            0);
    }

    void D3D12CommandList::SetComputePushConstants(Pipeline *pPipeline, void *pData, u32 dataSize)
    {
        ZoneScoped;

        API_VAR(D3D12Pipeline, pPipeline);

        m_pHandle->SetComputeRoot32BitConstants(
            pPipelineDX->m_pRootConstats[LR_SHADER_STAGE_COMPUTE], dataSize / sizeof(u32), pData, 0);
    }

}  // namespace lr::Graphics