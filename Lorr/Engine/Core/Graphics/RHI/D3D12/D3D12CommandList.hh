//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseCommandList.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12CommandAllocator : BaseCommandAllocator
    {
        void Reset()
        {
            pHandle->Reset();
        }

        ID3D12CommandAllocator *pHandle = nullptr;
    };

    struct D3D12CommandList : BaseCommandList
    {
        void Init(ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type);
        void Reset(D3D12CommandAllocator *pAllocator);

        void BeginPass(CommandListBeginDesc *pDesc) override;
        void EndPass() override;
        void ClearImage(BaseImage *pImage, ClearValue val) override;

        /// Memory Barriers
        void SetImageBarrier(BaseImage *pImage, PipelineBarrier *pBarrier) override;
        void SetBufferBarrier(BaseBuffer *pBuffer, PipelineBarrier *pBarrier) override;

        /// Buffer Commands
        void SetVertexBuffer(BaseBuffer *pBuffer) override;
        void SetIndexBuffer(BaseBuffer *pBuffer, bool type32 = true) override;
        void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size) override;
        void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest) override;

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1) override;
        void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 0) override;
        void Dispatch(u32 groupX, u32 groupY, u32 groupZ) override;

        /// Pipeline States
        void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetPrimitiveType(PrimitiveType type) override;

        void SetGraphicsPipeline(BasePipeline *pPipeline) override;
        void SetComputePipeline(BasePipeline *pPipeline) override;
        void SetGraphicsDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) override;
        void SetComputeDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) override;
        void SetGraphicsPushConstants(BasePipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize) override;
        void SetComputePushConstants(BasePipeline *pPipeline, void *pData, u32 dataSize) override;

        D3D12CommandAllocator *m_pAllocator = nullptr;
        ID3D12GraphicsCommandList4 *m_pHandle = nullptr;

        eastl::atomic<u64> m_DesiredFenceValue = 0;
        ID3D12Fence *m_pFence = nullptr;
        HANDLE m_FenceEvent = nullptr;
    };

}  // namespace lr::Graphics