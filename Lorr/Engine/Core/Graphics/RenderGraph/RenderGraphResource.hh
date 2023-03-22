//
// Created on Thursday 23rd February 2023 by exdal
//

#pragma once

#include "Core/Crypt/FNV.hh"

#include "Core/Graphics/Vulkan/VKContext.hh"
#include "Core/Graphics/Vulkan/VKPipeline.hh"
#include "Core/Graphics/Vulkan/VKResource.hh"

namespace lr::Graphics
{
using Hash64 = u64;
using NameID = eastl::string_view;

enum AttachmentFlags : u32
{
    LR_ATTACHMENT_FLAG_NONE = 0,
    LR_ATTACHMENT_FLAG_SC_SCALING = 1 << 0,  // Attachment's size is relative to swapchain size
    LR_ATTACHMENT_FLAG_SC_FORMAT = 1 << 1,   // Attachment's format is relative to swapchain format
    LR_ATTACHMENT_FLAG_CLEAR = 1 << 2,
};
EnumFlags(AttachmentFlags);

struct ColorAttachment
{
    Hash64 m_Hash = ~0;
};

struct DepthAttachment
{
    Hash64 m_Hash = ~0;
};

struct RenderGraphResourceManager
{
    /// TRACKED RESOURCES ///
    Image *CreateImage(NameID name, ImageDesc *pDesc);
    Buffer *CreateBuffer(NameID name, BufferDesc *pDesc);
    Sampler *CreateSampler(NameID name, SamplerDesc *pDesc);

    /// UNTRACKED RESOURCES ///
    DescriptorSetLayout *CreateDescriptorSetLayout(eastl::span<DescriptorLayoutElement> elements);
    DescriptorSet *CreateDescriptorSet(DescriptorSetLayout *pLayout);
    Shader *CreateShader(ShaderStage type, BufferReadStream &buf);
    Shader *CreateShader(ShaderStage type, NameID path);
    Pipeline *CreatePipeline(GraphicsPipelineBuildInfo &buildInfo);

    void DeleteImage(NameID name);
    void DeleteBuffer(NameID name);
    void DeleteBuffer(Buffer *pBuffer);
    void DeleteSampler(NameID name);
    void DeleteDescriptorSet(NameID name);
    void DeleteShader(Shader *pShader);

    void UpdateDescriptorSet(DescriptorSet *pSet, cinitl<DescriptorWriteData> &elements);
    void MapBuffer(Buffer *pBuffer, void *&pDataOut);
    void UnmapBuffer(Buffer *pBuffer);

    template<typename _Resource>
    _Resource *GetResource(NameID name)
    {
        return GetResource<_Resource>(Hash::FNV64String(name));
    }

    template<typename _Resource>
    _Resource *GetResource(Hash64 hash)
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
    void AddResource(NameID name, _Resource *pResource)
    {
        ZoneScoped;

        Hash64 hash = Hash::FNV64String(name);
        m_Resources.push_back(TrackedResource{
            .m_Hash = hash,
            .m_pTypelessHandle = pResource,
        });
    }

    // Internal
    template<typename _Resource>
    _Resource *RemoveResource(NameID name)
    {
        Hash64 hash = Hash::FNV64String(name);
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


};

}  // namespace lr::Graphics