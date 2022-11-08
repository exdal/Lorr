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

        void BarrierTransition(BaseImage *pImage,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter) override;

        void BarrierTransition(BaseBuffer *pBuffer,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter) override;

        void ClearImage(BaseImage *pImage, ClearValue val) override;

        void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetPrimitiveType(PrimitiveType type) override;

        /// Buffer Commands
        void SetVertexBuffer(BaseBuffer *pBuffer) override;
        void SetIndexBuffer(BaseBuffer *pBuffer, bool type32) override;
        void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size) override;
        void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest) override;

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance) override;
        void DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance) override;

        // Pipeline
        void SetPipeline(BasePipeline *pPipeline) override;
        void SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) override;

        D3D12CommandAllocator *m_pAllocator = nullptr;
        ID3D12GraphicsCommandList4 *m_pHandle = nullptr;

        eastl::atomic<u64> m_DesiredFenceValue = 0;
        ID3D12Fence *m_pFence = nullptr;
        HANDLE m_FenceEvent = nullptr;
    };

}  // namespace lr::Graphics