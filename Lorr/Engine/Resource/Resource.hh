// Created on Thursday November 10th 2022 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

namespace lr::Resource
{
enum class ResourceType : u32
{
    Data,  // Any data that is not fitting for anything under, for example world save, etc...
    Shader,
    Texture,
    Model,
};

struct ResourceData
{
    ResourceType m_Type = ResourceType::Data;
    u32 m_Size = 0;
};

}  // namespace lr::Resource