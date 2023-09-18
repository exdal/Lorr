// Created on Thursday November 10th 2022 by exdal
#pragma once

#include "Identifier.hh"

#include "Graphics/Shader.hh"

namespace lr::Resource
{
enum class ResourceType : u32
{
    Data,  // Any data that is not fitting for anything under, ie, world
    Shader,
    Texture,
    Model,
};

template<typename _Data, ResourceType _Type>
struct Resource
{
    static constexpr ResourceType kType = _Type;
    using DataType_t = _Data;

    _Data &get() { return m_Data; }
    u64 &size() { return m_Size; }

    Identifier m_Identifier;
    u64 m_Size = 0;
    _Data m_Data = {};
};

using ShaderResource = Resource<Graphics::ShaderCompileOutput, ResourceType::Shader>;

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