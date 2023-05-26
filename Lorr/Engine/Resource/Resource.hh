// Created on Thursday November 10th 2022 by exdal
// Last modified on Tuesday May 23rd 2023 by exdal

#pragma once

#include "Graphics/Shader.hh"

namespace lr::Resource
{
enum class ResourceType : u32
{
    Data,  // Any data that is not fitting for anything under, for example world save, etc...
    Shader,
    Texture,
    Model,
};

template<typename _Data, ResourceType _Type>
struct ResourceData
{
    typedef _Data type;

    _Data &get() { return m_Data; }
    u64 &size() { return m_Size; }
    ResourceType resourceType() { return m_Type; }

    ResourceType m_Type = _Type;
    u64 m_Size = 0;
    _Data m_Data = {};
};

using ShaderResource = Resource::ResourceData<Graphics::ShaderCompileOutput, Resource::ResourceType::Shader>;

template<typename _Resource>
struct ResourceWrapper;

template<>
struct ResourceWrapper<ShaderResource>
{
    static void Destroy(ShaderResource &resource)
    {
        ZoneScoped;

        Graphics::ShaderCompiler::FreeProgram(resource.get());
    }
};

}  // namespace lr::Resource