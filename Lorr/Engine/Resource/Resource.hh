// Created on Thursday November 10th 2022 by exdal
// Last modified on Thursday May 18th 2023 by exdal

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
    _Data m_Data = nullptr;
};

using ShaderResource = Resource::ResourceData<Graphics::ShaderCompileOutput, Resource::ResourceType::Shader>;

}  // namespace lr::Resource