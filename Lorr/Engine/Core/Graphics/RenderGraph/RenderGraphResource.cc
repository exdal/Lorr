#include "RenderGraphResource.hh"

namespace lr::Graphics
{
Image *RenderGraphResourceManager::CreateImage(eastl::string_view name, ImageDesc *pDesc)
{
    ZoneScoped;

    Image *pImage = m_pAPI->CreateImage(pDesc);
    if (name != "$notrack")
    {
        ResourceID hash = Hash::FNV64String(name);
        m_Resources.push_back(TrackedResource{ .m_Hash = hash, .m_pImage = pImage });

        LOG_TRACE("Created new IMAGE {}@{}.", name, hash);
    }

    return pImage;
}

Buffer *RenderGraphResourceManager::CreateBuffer(eastl::string_view name, BufferDesc *pDesc)
{
    ZoneScoped;

    Buffer *pBuffer = m_pAPI->CreateBuffer(pDesc);
    if (name != "$notrack")
    {
        ResourceID hash = Hash::FNV64String(name);
        m_Resources.push_back(TrackedResource{ .m_Hash = hash, .m_pBuffer = pBuffer });

        LOG_TRACE("Created new BUFFER {}@{}.", name, hash);
    }

    return pBuffer;
}

Sampler *RenderGraphResourceManager::CreateSampler(eastl::string_view name, SamplerDesc *pDesc)
{
    ZoneScoped;

    Sampler *pSampler = m_pAPI->CreateSampler(pDesc);
    ResourceID hash = Hash::FNV64String(name);
    m_Resources.push_back(TrackedResource{ .m_Hash = hash, .m_pSampler = pSampler });

    LOG_TRACE("Created new SAMPLER {}@{}.", name, hash);

    return pSampler;
}

DescriptorSetLayout *RenderGraphResourceManager::CreateDescriptorSetLayout(
    eastl::span<DescriptorLayoutElement> elements)
{
    ZoneScoped;

    return m_pAPI->CreateDescriptorSetLayout(elements);
}

DescriptorSet *RenderGraphResourceManager::CreateDescriptorSet(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    return m_pAPI->CreateDescriptorSet(pLayout);
}

Shader *RenderGraphResourceManager::CreateShader(ShaderStage type, BufferReadStream &buf)
{
    ZoneScoped;

    return m_pAPI->CreateShader(type, buf);
}

Shader *RenderGraphResourceManager::CreateShader(ShaderStage type, eastl::string_view path)
{
    ZoneScoped;

    return m_pAPI->CreateShader(type, path);
}

Pipeline *RenderGraphResourceManager::CreatePipeline(GraphicsPipelineBuildInfo &buildInfo)
{
    ZoneScoped;

    return m_pAPI->CreateGraphicsPipeline(&buildInfo);
}

void RenderGraphResourceManager::DeleteImage(eastl::string_view name)
{
    ZoneScoped;

    Image *pResource = RemoveResource<Image>(name);
    if (!pResource)
        LOG_CRITICAL("Resource '{}' is not found in registry.", name);

    m_pAPI->DeleteImage(pResource);
}

void RenderGraphResourceManager::DeleteBuffer(eastl::string_view name)
{
    ZoneScoped;

    Buffer *pResource = RemoveResource<Buffer>(name);
    if (!pResource)
        LOG_CRITICAL("Resource '{}' is not found in registry.", name);

    m_pAPI->DeleteBuffer(pResource);
}

void RenderGraphResourceManager::DeleteBuffer(Buffer *pBuffer)
{
    ZoneScoped;

    m_pAPI->DeleteBuffer(pBuffer);
}

void RenderGraphResourceManager::DeleteSampler(eastl::string_view name)
{
    ZoneScoped;

    Sampler *pResource = RemoveResource<Sampler>(name);
    if (!pResource)
        LOG_CRITICAL("Resource '{}' is not found in registry.", name);

    // m_pAPI->DeleteSampler(pResource);
}

void RenderGraphResourceManager::DeleteDescriptorSet(eastl::string_view name)
{
    ZoneScoped;

    DescriptorSet *pResource = RemoveResource<DescriptorSet>(name);
    if (!pResource)
        LOG_CRITICAL("Resource '{}' is not found in registry.", name);

    m_pAPI->DeleteDescriptorSet(pResource);
}

void RenderGraphResourceManager::DeleteShader(Shader *pShader)
{
    ZoneScoped;

    m_pAPI->DeleteShader(pShader);
}

void RenderGraphResourceManager::UpdateDescriptorSet(DescriptorSet *pSet, cinitl<DescriptorWriteData> &elements)
{
    ZoneScoped;

    m_pAPI->UpdateDescriptorSet(pSet, elements);
}

void RenderGraphResourceManager::MapBuffer(Buffer *pBuffer, void *&pDataOut)
{
    ZoneScoped;

    m_pAPI->MapMemory(pBuffer, pDataOut);
}

void RenderGraphResourceManager::UnmapBuffer(Buffer *pBuffer)
{
    ZoneScoped;

    m_pAPI->UnmapMemory(pBuffer);
}

}  // namespace lr::Graphics