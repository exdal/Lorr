//
// Created on Thursday 23rd February 2023 by exdal
//

#pragma once

#include "Core/Crypto/FNV.hh"

#include "Core/Graphics/Vulkan/VKAPI.hh"
#include "Core/Graphics/Vulkan/VKPipeline.hh"
#include "Core/Graphics/Vulkan/VKResource.hh"

namespace lr::Graphics
{
struct RenderGraphResourceManager
{
    /// TRACKED RESOURCES ///
    Image *CreateImage(eastl::string_view name, ImageDesc *pDesc);
    Buffer *CreateBuffer(eastl::string_view name, BufferDesc *pDesc);
    Sampler *CreateSampler(eastl::string_view name, SamplerDesc *pDesc);

    /// UNTRACKED RESOURCES ///
    DescriptorSetLayout *CreateDescriptorSetLayout(eastl::span<DescriptorLayoutElement> elements);
    DescriptorSet *CreateDescriptorSet(DescriptorSetLayout *pLayout);
    Shader *CreateShader(ShaderStage type, BufferReadStream &buf);
    Shader *CreateShader(ShaderStage type, eastl::string_view path);
    Pipeline *CreatePipeline(GraphicsPipelineBuildInfo &buildInfo);

    void DeleteImage(eastl::string_view name);
    void DeleteBuffer(eastl::string_view name);
    void DeleteBuffer(Buffer *pBuffer);
    void DeleteSampler(eastl::string_view name);
    void DeleteDescriptorSet(eastl::string_view name);
    void DeleteShader(Shader *pShader);

    void UpdateDescriptorSet(DescriptorSet *pSet, cinitl<DescriptorWriteData> &elements);
    void MapBuffer(Buffer *pBuffer, void *&pDataOut);
    void UnmapBuffer(Buffer *pBuffer);

    template<typename _Resource>
    _Resource *GetResource(eastl::string_view name)
    {
        return GetResource<_Resource>(Hash::FNV64String(name));
    }

    template<typename _Resource>
    _Resource *GetResource(ResourceID hash)
    {
        ZoneScoped;

        switch (hash)
        {
            case FNV64HashOf("$backbuffer"):
                return m_pAPI->GetCurrentFrame()->m_pImage;
            default:
                break;
        }

        for (TrackedResource &resource : m_Resources)
            if (hash == resource.m_Hash)
                return (_Resource *)resource.m_pTypelessHandle;

        return nullptr;
    }

    template<typename _Resource>
    void AddResource(eastl::string_view name, _Resource *pResource)
    {
        ZoneScoped;

        ResourceID hash = Hash::FNV64String(name);
        m_Resources.push_back(TrackedResource{
            .m_Hash = hash,
            .m_pTypelessHandle = pResource,
        });
    }

    // Internal
    template<typename _Resource>
    _Resource *RemoveResource(eastl::string_view name)
    {
        ResourceID hash = Hash::FNV64String(name);
        _Resource *pResource = nullptr;

        for (u32 i = 0; i < m_Resources.size(); i++)
        {
            TrackedResource &res = m_Resources[i];
            if (res.m_Hash == hash)
            {
                pResource = (_Resource *)res.m_pTypelessHandle;
                m_Resources.erase(m_Resources.begin() + i);
                break;
            }
        }

        return pResource;
    }

    VKAPI *m_pAPI = nullptr;

    struct TrackedResource
    {
        ResourceID m_Hash = 0;

        union
        {
            Image *m_pImage = nullptr;
            Buffer *m_pBuffer;
            Sampler *m_pSampler;
            void *m_pTypelessHandle;  // for templates
        };
    };

    eastl::vector<TrackedResource> m_Resources;
};

}  // namespace lr::Graphics