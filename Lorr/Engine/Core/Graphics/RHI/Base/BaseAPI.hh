//
// Created on Sunday 23rd October 2022 by e-erdal
//

#pragma once

#include <eathread/eathread_thread.h>

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "Core/IO/BufferStream.hh"
#include "Core/IO/Memory.hh"
#include "Core/IO/MemoryAllocator.hh"

#include "BasePipeline.hh"

#include "BaseCommandQueue.hh"
#include "BaseCommandList.hh"
#include "BaseSwapChain.hh"
#include "BaseRenderPass.hh"

namespace lr::Graphics
{
    //* Not fully virtual struct that we *only* expose external functions.

    struct BaseAPI
    {
        virtual bool Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags) = 0;

        void InitCommandLists();
        virtual void InitAllocators() = 0;

        /// COMMAND ///
        virtual BaseCommandQueue *CreateCommandQueue(CommandListType type) = 0;
        virtual BaseCommandAllocator *CreateCommandAllocator(CommandListType type) = 0;
        virtual BaseCommandList *CreateCommandList(CommandListType type) = 0;

        BaseCommandList *GetCommandList();

        virtual void BeginCommandList(BaseCommandList *pList) = 0;
        virtual void EndCommandList(BaseCommandList *pList) = 0;
        virtual void ResetCommandAllocator(BaseCommandAllocator *pAllocator) = 0;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        virtual void ExecuteCommandList(BaseCommandList *pList, bool waitForFence) = 0;

        /// RENDERPASS ///
        virtual BaseRenderPass *CreateRenderPass(RenderPassDesc *pDesc) = 0;
        virtual void DeleteRenderPass(BaseRenderPass *pRenderPass) = 0;

        /// PIPELINE ///
        virtual void BeginPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo) = 0;
        virtual BasePipeline *EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo, BaseRenderPass *pRenderPass) = 0;

        /// SWAPCHAIN ///
        virtual void ResizeSwapChain(u32 width, u32 height) = 0;

        virtual void Frame() = 0;
        virtual void WaitForDevice() = 0;

        /// RESOURCE ///
        virtual BaseDescriptorSet *CreateDescriptorSet(DescriptorSetDesc *pDesc) = 0;
        virtual void UpdateDescriptorData(BaseDescriptorSet *pSet, DescriptorSetDesc *pDesc) = 0;

        template<typename T>
        T *AllocType()
        {
            Memory::TLSFBlock *pBlock = nullptr;
            u32 offset = m_TypeAllocator.Allocate(sizeof(T) + PTR_SIZE, Memory::TLSFMemoryAllocator::ALIGN_SIZE_LOG2, pBlock);

            memcpy(m_pTypeData + offset, pBlock, PTR_SIZE);

            return (T *)(m_pTypeData + offset + PTR_SIZE);
        }

        template<typename T>
        void FreeType(T *&pType)
        {
            assert(pType && "You cannot free non-existing memory.");

            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)((u8 *)pType - PTR_SIZE);
            m_TypeAllocator.Free(pBlock);

            Memory::ZeroMem((u8 *)pBlock, sizeof(T) + PTR_SIZE);

            pType = nullptr;
        }

        Memory::TLSFMemoryAllocator m_TypeAllocator;
        u8 *m_pTypeData = nullptr;

        CommandListPool m_CommandListPool;
    };

}  // namespace lr::Graphics