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
    struct BaseAPI
    {
        virtual bool Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags) = 0;

        virtual void InitCommandLists() = 0;
        virtual void InitAllocators() = 0;

        /// COMMAND ///
        virtual void CreateCommandQueue(BaseCommandQueue *pQueue, CommandListType type) = 0;
        virtual void CreateCommandAllocator(BaseCommandAllocator *pAllocator, CommandListType type) = 0;
        virtual void CreateCommandList(BaseCommandList *pList, CommandListType type) = 0;

        virtual BaseCommandList *GetCommandList() = 0;

        virtual void BeginCommandList(BaseCommandList *pList) = 0;
        virtual void EndCommandList(BaseCommandList *pList) = 0;
        virtual void ResetCommandAllocator(BaseCommandAllocator *pAllocator) = 0;

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        virtual void ExecuteCommandList(BaseCommandList *pList, bool waitForFence) = 0;
        // Executes command list of current frame, it always performs a present operation.
        virtual void PresentCommandQueue() = 0;

        /// RENDERPASS ///
        virtual void CreateRenderPass(BaseRenderPass *&pRenderPass, RenderPassDesc *pDesc) = 0;
        virtual void DeleteRenderPass(BaseRenderPass *pRenderPass) = 0;

        /// PIPELINE ///
        virtual void BeginPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo) = 0;
        virtual void EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo, BaseRenderPass *pRenderPass, BasePipeline *pTargetHandle) = 0;

        /// SWAPCHAIN ///
        virtual void ResizeSwapChain(u32 width, u32 height) = 0;

        virtual void Frame() = 0;
        virtual void WaitForDevice() = 0;

        template<typename T>
        void AllocType(T *&pType)
        {
            Memory::TLSFBlock *pBlock = nullptr;
            u32 offset = m_TypeAllocator.Allocate(sizeof(T) + sizeof(void *), Memory::TLSFMemoryAllocator::ALIGN_SIZE_LOG2, pBlock);

            memcpy(m_pTypeData + offset, pBlock, sizeof(void *));

            pType = (T *)(m_pTypeData + offset + sizeof(void *));
        }

        template<typename T>
        void FreeType(T *pType)
        {
            assert(pType && "You cannot free non-existing memory.");

            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)((u8 *)pType - sizeof(void *));
            m_TypeAllocator.Free(pBlock);

            Memory::ZeroMem((u8 *)pBlock, sizeof(T) + sizeof(void *));
        }

        Memory::TLSFMemoryAllocator m_TypeAllocator;
        u8 *m_pTypeData = nullptr;
    };

}  // namespace lr::Graphics