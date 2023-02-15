//
// Created on Sunday 23rd October 2022 by e-erdal
//

#pragma once

#include <eathread/eathread_thread.h>

#include "Core/Graphics/Camera.hh"
#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "Core/IO/BufferStream.hh"

#include "Core/Memory/Allocator/LinearAllocator.hh"
#include "Core/Memory/Allocator/TLSFAllocator.hh"

#include "BasePipeline.hh"

#include "BaseCommandList.hh"
#include "BaseSwapChain.hh"

namespace lr::Graphics
{
    struct PipelineLayoutSerializeDesc
    {
        u32 m_DescriptorSetCount = 0;
        DescriptorSet **m_ppDescriptorSets = nullptr;
        u32 m_PushConstantCount = 0;
        PushConstantDesc *m_pPushConstants = nullptr;
    };

    struct APIAllocatorInitDesc
    {
        u32 m_MaxTLSFAllocations = 0x2000;
        u32 m_DescriptorMem = Memory::MiBToBytes(1);
        u32 m_BufferLinearMem = Memory::MiBToBytes(64);
        u32 m_BufferTLSFMem = Memory::MiBToBytes(1024);
        u32 m_BufferTLSFHostMem = Memory::MiBToBytes(128);
        u32 m_BufferFrametimeMem = Memory::MiBToBytes(128);
        u32 m_ImageTLSFMem = Memory::MiBToBytes(1024);
    };

    struct BaseAPIDesc
    {
        APIFlags m_Flags = LR_API_FLAG_NONE;
        SwapChainFlags m_SwapChainFlags = LR_SWAP_CHAIN_FLAG_NONE;
        BaseWindow *m_pTargetWindow = nullptr;
        APIAllocatorInitDesc m_AllocatorDesc = {};
    };

    //* Not fully virtual struct that we *only* expose external functions.

    struct BaseAPI
    {
        virtual bool Init(BaseAPIDesc *pDesc) = 0;
        virtual void InitAllocators(APIAllocatorInitDesc *pDesc) = 0;

        /// COMMAND ///
        CommandList *GetCommandList(CommandListType type = CommandListType::Direct);

        virtual void BeginCommandList(CommandList *pList) = 0;
        virtual void EndCommandList(CommandList *pList) = 0;
        virtual void ResetCommandAllocator(CommandAllocator *pAllocator) = 0;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        virtual void ExecuteCommandList(CommandList *pList, bool waitForFence) = 0;

        /// PIPELINE ///
        virtual Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo) = 0;
        virtual Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo) = 0;

        /// SWAPCHAIN ///
        virtual void ResizeSwapChain(u32 width, u32 height) = 0;
        virtual BaseSwapChain *GetSwapChain() = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        /// RESOURCE ///
        virtual DescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) = 0;
        // Only deletes immutable samplers
        virtual void DeleteDescriptorSet(DescriptorSet *pSet) = 0;
        virtual void UpdateDescriptorData(DescriptorSet *pSet, DescriptorSetDesc *pDesc) = 0;

        virtual Shader *CreateShader(ShaderStage stage, BufferReadStream &buf) = 0;
        virtual Shader *CreateShader(ShaderStage stage, eastl::string_view path) = 0;
        virtual void DeleteShader(Shader *pShader) = 0;

        // * Buffers * //
        virtual Buffer *CreateBuffer(BufferDesc *pDesc) = 0;
        virtual void DeleteBuffer(Buffer *pBuffer) = 0;

        virtual void MapMemory(Buffer *pBuffer, void *&pData) = 0;
        virtual void UnmapMemory(Buffer *pBuffer) = 0;

        // * Images * //
        virtual Image *CreateImage(ImageDesc *pDesc) = 0;
        virtual void DeleteImage(Image *pImage) = 0;
        virtual Sampler *CreateSampler(SamplerDesc *pDesc) = 0;

        /// UTILITY
        virtual ImageFormat &GetSwapChainImageFormat() = 0;
        virtual void CalcOrthoProjection(Camera2D &camera) = 0;
        virtual void CalcPerspectiveProjection(Camera3D &camera) = 0;

        template<typename _Base, typename _Derived>
        _Derived *AllocTypeInherit()
        {
            static_assert(eastl::is_base_of<_Base, _Derived>::value, "_Derived must be base of _Base.");

            Memory::TLSFBlock *pBlock =
                m_TypeAllocator.Allocate(sizeof(_Derived) + PTR_SIZE, Memory::TLSFAllocatorView::ALIGN_SIZE);
            u32 offset = pBlock->m_Offset;

            // Copy block address
            memcpy(m_pTypeData + offset, &pBlock, PTR_SIZE);
            offset += PTR_SIZE;

            void *pBaseAddr = m_pTypeData + offset;
            _Base *pBase = new (pBaseAddr) _Derived;

            return (_Derived *)pBase;
        }

        template<typename _Type>
        _Type *AllocType()
        {
            Memory::TLSFBlock *pBlock =
                m_TypeAllocator.Allocate(sizeof(_Type) + PTR_SIZE, Memory::TLSFAllocatorView::ALIGN_SIZE);

            memcpy(m_pTypeData + pBlock->m_Offset, &pBlock, PTR_SIZE);
            return (_Type *)(m_pTypeData + pBlock->m_Offset + PTR_SIZE);
        }

        template<typename _Type>
        void FreeType(_Type *&pType)
        {
            assert(pType && "You cannot free non-existing memory.");

            Memory::TLSFBlock *pBlock = *(Memory::TLSFBlock **)((u8 *)pType - PTR_SIZE);
            m_TypeAllocator.Free(pBlock);

            pType = nullptr;
        }

        /// ----------------------------------------------------------- ///

        template<typename _Allocator, typename _Heap>
        struct BufferedAllocator
        {
            _Allocator Allocator;
            _Heap pHeap;
        };

        /// ----------------------------------------------------------- ///

        Memory::TLSFAllocatorView m_TypeAllocator;
        u8 *m_pTypeData = nullptr;

        CommandListPool m_CommandListPool = {};
    };

}  // namespace lr::Graphics