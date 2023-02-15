//
// Created on Friday 11th November 2022 by e-erdal
//

#pragma once

#include "Core/Crypto/TypeHash.hh"
#include "Core/Graphics/RHI/Base/BaseAPI.hh"

namespace lr::Renderer
{
    /*
    void InitImguiPass(Image *BackBuffer)
    {
        struct PassData
        {
            Image *m_pBackbuffer;
        };

        m_RenderGraph.AddGraphicsPass<PassData>("imgui_pass",
            [&](RenderPassResourceManager &resourceMan, PassData &data)     /// <-- Setup
            {
                resourceMan.PushAttachment({attachment args});
                resourceMan.SetPipeline();

                data.m_pBackbuffer = resourceMan.TrackImage("backbuffer", pBackBuffer);
            },
            [=](PassData &data, CommandList *pList)                         /// <-- Execute
            {
                Image *pImage = data.m_pBackBuffer;

                pList->Draw(...);
            },
            [](RenderPassResourceManager &)                                 /// <-- Clear
            {
                resources.Delete("backbuffer"); // dont delete backbuffer, its for example
            });
    }*/

    // Normally I wouldn't create new abstraction for API to keep it low level
    // but I needed something that would keep track of API resources, and I believe
    // using this class for rendergraph is more reasonable than just passing BaseAPI
    using namespace Graphics;
    struct RenderPassResourceManager
    {
        Image *CreateImage(eastl::string_view name, ImageDesc *pDesc);
        Buffer *CreateBuffer(eastl::string_view name, BufferDesc *pDesc);
        Sampler *CreateSampler(eastl::string_view name, SamplerDesc *pDesc);
        Pipeline *CreateGraphicsPipeline(eastl::string_view name, GraphicsPipelineBuildInfo &info);
        DescriptorSet *CreateDescriptorSet(eastl::string_view name, DescriptorSetDesc *pDesc);

        Image *CreateBufferedImage(eastl::string_view name, ImageDesc *pDesc, void *pData);
        Shader *CreateShader(ShaderStage type, BufferReadStream &buf);

        void DeleteImage(eastl::string_view name);
        void DeleteBuffer(eastl::string_view name);
        void DeleteSampler(eastl::string_view name);
        void DeletePipeline(eastl::string_view name);
        void DeleteDescriptorSet(eastl::string_view name);
        void DeleteShader(Shader *pShader);

        void MapBuffer(Buffer *pBuffer, void *&pDataOut);
        void UnmapBuffer(Buffer *pBuffer);

        ImageFormat GetSwapChainFormat();
        Image *GetSwapChainImage();  // Current index image

        // Internal
        template<typename _Resource>
        _Resource *RemoveResource(eastl::string_view name)
        {
            u64 hash = Hash::FNV64String(name);
            _Resource *pResource = nullptr;

            for (u32 i = 0; i < m_Resources.size(); i++)
            {
                Resource &res = m_Resources[i];
                if (res.m_Hash == hash)
                {
                    pResource = (_Resource *)res.m_pUniHandle;
                    m_Resources.erase(m_Resources.begin() + i);
                    break;
                }
            }

            return pResource;
        }

        BaseAPI *m_pAPI = nullptr;

        struct Resource
        {
            u64 m_Hash = 0;

            union
            {
                Image *m_pImage = nullptr;
                Buffer *m_pBuffer;
                Sampler *m_pSampler;
                Pipeline *m_pPipeline;
                DescriptorSet *m_pDescriptor;
                void *m_pUniHandle;  // for templates
            };
        };

        eastl::vector<Resource> m_Resources;
    };

    template<typename _Data>
    using RenderPassSetupFn = eastl::function<void(RenderPassResourceManager &, _Data &, CommandList *)>;
    template<typename _Data>
    using RenderPassExecuteFn = eastl::function<void(RenderPassResourceManager &, _Data &, CommandList *)>;
    using RenderPassShutdownFn = eastl::function<void(RenderPassResourceManager &)>;

    struct RenderPass
    {
        u64 m_PassNameHash;
        CommandListType m_PassType;

        virtual void Execute(RenderPassResourceManager &resourceMan, CommandList *pList) = 0;
        virtual void Shutdown(RenderPassResourceManager &resourceMan) = 0;
    };

    template<typename _Data>
    struct GraphicsRenderPass : RenderPass, _Data
    {
        GraphicsRenderPass(
            eastl::string_view name,
            RenderPassResourceManager &resourceMan,
            CommandList *pList,
            const RenderPassSetupFn<_Data> &fSetup,
            const RenderPassExecuteFn<_Data> &fExecute,
            const RenderPassShutdownFn &fShutdown)
        {
            m_PassNameHash = Hash::FNV64String(name);
            m_PassType = CommandListType::Direct;
            m_fExecute = fExecute;
            m_fShutdown = fShutdown;

            LOG_TRACE("Setting up {}@{}...", name, m_PassNameHash);
            fSetup(resourceMan, (_Data &)(*this), pList);
        }

        void Execute(RenderPassResourceManager &resourceMan, CommandList *pList) override
        {
            m_fExecute(resourceMan, (_Data &)(*this), pList);
        }

        void Shutdown(RenderPassResourceManager &resourceMan) override
        {
            m_fShutdown(resourceMan);
        }

        RenderPassExecuteFn<_Data> m_fExecute;
        RenderPassShutdownFn m_fShutdown;
    };

    struct RenderGraphDesc
    {
        BaseAPI *m_pAPI = nullptr;
    };

    struct RenderGraph
    {
        void Init(RenderGraphDesc *pDesc);
        void Shutdown();

        void Draw();

        template<typename _Data>
        void AddGraphicsPass(
            eastl::string_view name,
            RenderPassSetupFn<_Data> fSetup,
            RenderPassExecuteFn<_Data> fExecute,
            RenderPassShutdownFn fShutdown)
        {
            Memory::AllocationInfo info = {
                .m_Size = sizeof(GraphicsRenderPass<_Data>),
                .m_Alignment = 8,
            };

            m_Allocator.Allocate(info);

            RenderPass *pPass = new (info.m_pData)
                GraphicsRenderPass<_Data>(name, m_ResourceManager, nullptr, fSetup, fExecute, fShutdown);

            m_Passes.push_back(pPass);
        }

        RenderPassResourceManager m_ResourceManager = {};
        BaseAPI *m_pAPI = nullptr;

        Memory::LinearAllocator m_Allocator = {};
        eastl::vector<RenderPass *> m_Passes;
    };

}  // namespace lr::Renderer