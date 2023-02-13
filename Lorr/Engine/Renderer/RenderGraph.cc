#include "RenderGraph.hh"

#undef LOG_SET_NAME
#define LOG_SET_NAME "RENDERGRAPH"

namespace lr::Renderer
{
    Image *RenderPassResourceManager::CreateImage(eastl::string_view name, ImageDesc *pDesc)
    {
        ZoneScoped;

        Image *pImage = m_pAPI->CreateImage(pDesc);
        u64 hash = Hash::FNV64String(name);
        m_Resources.push_back(Resource{ .m_Hash = hash, .m_pImage = pImage });

        LOG_TRACE("Created new IMAGE {}@{}.", name, hash);

        return pImage;
    }

    Buffer *RenderPassResourceManager::CreateBuffer(eastl::string_view name, BufferDesc *pDesc)
    {
        ZoneScoped;

        Buffer *pBuffer = m_pAPI->CreateBuffer(pDesc);
        u64 hash = Hash::FNV64String(name);
        m_Resources.push_back(Resource{ .m_Hash = hash, .m_pBuffer = pBuffer });

        LOG_TRACE("Created new BUFFER {}@{}.", name, hash);

        return pBuffer;
    }

    Sampler *RenderPassResourceManager::CreateSampler(eastl::string_view name, SamplerDesc *pDesc)
    {
        ZoneScoped;

        Sampler *pSampler = m_pAPI->CreateSampler(pDesc);
        u64 hash = Hash::FNV64String(name);
        m_Resources.push_back(Resource{ .m_Hash = hash, .m_pSampler = pSampler });

        LOG_TRACE("Created new SAMPLER {}@{}.", name, hash);

        return pSampler;
    }

    Pipeline *RenderPassResourceManager::CreateGraphicsPipeline(
        eastl::string_view name, GraphicsPipelineBuildInfo &info)
    {
        ZoneScoped;

        Pipeline *pPipeline = m_pAPI->CreateGraphicsPipeline(&info);
        u64 hash = Hash::FNV64String(name);
        m_Resources.push_back(Resource{ .m_Hash = hash, .m_pPipeline = pPipeline });

        LOG_TRACE("Created new GPIPELINE {}@{}.", name, hash);

        return pPipeline;
    }

    DescriptorSet *RenderPassResourceManager::CreateDescriptorSet(
        eastl::string_view name, DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        DescriptorSet *pDescriptorSet = m_pAPI->CreateDescriptorSet(pDesc);
        m_pAPI->UpdateDescriptorData(pDescriptorSet, pDesc);
        u64 hash = Hash::FNV64String(name);
        m_Resources.push_back(Resource{ .m_Hash = hash, .m_pDescriptor = pDescriptorSet });

        LOG_TRACE("Created new DESCRIPTORSET {}@{}.", name, hash);

        return pDescriptorSet;
    }

    Image *RenderPassResourceManager::CreateBufferedImage(
        eastl::string_view name, ImageDesc *pDesc, void *pData)
    {
        ZoneScoped;

        Image *pImage = CreateImage(name, pDesc);

        BufferDesc imageBufferDesc = {
            .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_SRC | LR_RESOURCE_USAGE_HOST_VISIBLE,
            .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME,
            .m_DataLen = pDesc->m_Width * pDesc->m_Height * GetImageFormatSize(pDesc->m_Format),
        };

        Buffer *pImageBuffer = m_pAPI->CreateBuffer(&imageBufferDesc);

        void *pMapData = nullptr;
        m_pAPI->MapMemory(pImageBuffer, pMapData);
        memcpy(pMapData, pData, imageBufferDesc.m_DataLen);
        m_pAPI->UnmapMemory(pImageBuffer);

        CommandList *pList = m_pAPI->GetCommandList();
        m_pAPI->BeginCommandList(pList);

        PipelineBarrier barrier = {
            .m_CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .m_CurrentStage = LR_PIPELINE_STAGE_HOST,
            .m_CurrentAccess = LR_PIPELINE_ACCESS_HOST_READ,
            .m_NextUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .m_NextStage = LR_PIPELINE_STAGE_TRANSFER,
            .m_NextAccess = LR_PIPELINE_ACCESS_TRANSFER_WRITE,
        };
        pList->SetImageBarrier(pImage, &barrier);

        pList->CopyBuffer(pImageBuffer, pImage);

        barrier = {
            .m_CurrentUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .m_CurrentStage = LR_PIPELINE_STAGE_TRANSFER,
            .m_CurrentAccess = LR_PIPELINE_ACCESS_TRANSFER_WRITE,
            .m_NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .m_NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
            .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        };
        pList->SetImageBarrier(pImage, &barrier);

        m_pAPI->EndCommandList(pList);
        m_pAPI->ExecuteCommandList(pList, true);

        m_pAPI->DeleteBuffer(pImageBuffer);

        return pImage;
    }

    Shader *RenderPassResourceManager::CreateShader(ShaderStage type, BufferReadStream &buf)
    {
        ZoneScoped;

        return m_pAPI->CreateShader(type, buf);
    }

    void RenderPassResourceManager::DeleteImage(eastl::string_view name)
    {
        ZoneScoped;

        Image *pResource = RemoveResource<Image>(name);
        if (!pResource)
            LOG_CRITICAL("Resource '{}' is not found in registry.", name);

        m_pAPI->DeleteImage(pResource);
    }

    void RenderPassResourceManager::DeleteBuffer(eastl::string_view name)
    {
        ZoneScoped;

        Buffer *pResource = RemoveResource<Buffer>(name);
        if (!pResource)
            LOG_CRITICAL("Resource '{}' is not found in registry.", name);

        m_pAPI->DeleteBuffer(pResource);
    }

    void RenderPassResourceManager::DeleteSampler(eastl::string_view name)
    {
        ZoneScoped;

        Sampler *pResource = RemoveResource<Sampler>(name);
        if (!pResource)
            LOG_CRITICAL("Resource '{}' is not found in registry.", name);

        // m_pAPI->DeleteSampler(pResource);
    }

    void RenderPassResourceManager::DeletePipeline(eastl::string_view name)
    {
        ZoneScoped;

        Pipeline *pResource = RemoveResource<Pipeline>(name);
        if (!pResource)
            LOG_CRITICAL("Resource '{}' is not found in registry.", name);

        // m_pAPI->DeletePipeline(pResource);
    }

    void RenderPassResourceManager::DeleteDescriptorSet(eastl::string_view name)
    {
        ZoneScoped;

        DescriptorSet *pResource = RemoveResource<DescriptorSet>(name);
        if (!pResource)
            LOG_CRITICAL("Resource '{}' is not found in registry.", name);

        m_pAPI->DeleteDescriptorSet(pResource);
    }

    void RenderPassResourceManager::DeleteShader(Shader *pShader)
    {
        ZoneScoped;

        m_pAPI->DeleteShader(pShader);
    }

    void RenderPassResourceManager::MapBuffer(Buffer *pBuffer, void *&pDataOut)
    {
        ZoneScoped;

        m_pAPI->MapMemory(pBuffer, pDataOut);
    }

    void RenderPassResourceManager::UnmapBuffer(Buffer *pBuffer)
    {
        ZoneScoped;

        m_pAPI->UnmapMemory(pBuffer);
    }

    ImageFormat RenderPassResourceManager::GetSwapChainFormat()
    {
        ZoneScoped;

        return m_pAPI->GetSwapChainImageFormat();
    }

    Image *RenderPassResourceManager::GetSwapChainImage()
    {
        ZoneScoped;

        return m_pAPI->GetSwapChain()->GetCurrentImage();
    }

    void RenderGraph::Init(RenderGraphDesc *pDesc)
    {
        ZoneScoped;

        Memory::AllocatorDesc allocatorDesc = {
            .m_DataSize = Memory::KiBToBytes(64),
            .m_AutoGrowSize = Memory::KiBToBytes(6),
        };

        m_Allocator.Init(allocatorDesc);

        m_ResourceManager.m_pAPI = pDesc->m_pAPI;
        m_ResourceManager.m_Resources.reserve(Memory::KiBToBytes(16));
        m_pAPI = pDesc->m_pAPI;
    }

    void RenderGraph::Shutdown()
    {
        ZoneScoped;
    }

    void RenderGraph::Draw()
    {
        ZoneScoped;

        Image *pCurrentImage = m_pAPI->GetSwapChain()->GetCurrentImage();

        {
            CommandList *pList = m_pAPI->GetCommandList();
            m_pAPI->BeginCommandList(pList);

            PipelineBarrier barrier = {
                .m_CurrentUsage = LR_RESOURCE_USAGE_UNKNOWN,
                .m_CurrentStage = LR_PIPELINE_STAGE_NONE,
                .m_CurrentAccess = LR_PIPELINE_ACCESS_NONE,
                .m_NextUsage = LR_RESOURCE_USAGE_RENDER_TARGET,
                .m_NextStage = LR_PIPELINE_STAGE_RENDER_TARGET,
                .m_NextAccess = LR_PIPELINE_ACCESS_RENDER_TARGET_WRITE,
            };
            pList->SetImageBarrier(pCurrentImage, &barrier);

            m_pAPI->EndCommandList(pList);
            m_pAPI->ExecuteCommandList(pList, false);
        }

        for (RenderPass *&pPass : m_Passes)
        {
            CommandList *pList = m_pAPI->GetCommandList(pPass->m_PassType);
            m_pAPI->BeginCommandList(pList);
            pPass->Execute(m_ResourceManager, pList);
            m_pAPI->EndCommandList(pList);
            m_pAPI->ExecuteCommandList(pList, false);
        }

        {
            CommandList *pList = m_pAPI->GetCommandList();
            m_pAPI->BeginCommandList(pList);

            PipelineBarrier barrier = {
                .m_CurrentUsage = LR_RESOURCE_USAGE_RENDER_TARGET,
                .m_CurrentStage = LR_PIPELINE_STAGE_RENDER_TARGET,
                .m_CurrentAccess = LR_PIPELINE_ACCESS_RENDER_TARGET_WRITE,
                .m_NextUsage = LR_RESOURCE_USAGE_PRESENT,
                .m_NextStage = LR_PIPELINE_STAGE_NONE,
                .m_NextAccess = LR_PIPELINE_ACCESS_NONE,
            };
            pList->SetImageBarrier(pCurrentImage, &barrier);

            m_pAPI->EndCommandList(pList);
            m_pAPI->ExecuteCommandList(pList, false);
        }
    }

}  // namespace lr::Renderer