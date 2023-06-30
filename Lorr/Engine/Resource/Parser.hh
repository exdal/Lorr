// Created on Thursday May 18th 2023 by exdal
// Last modified on Tuesday June 27th 2023 by exdal

#pragma once

#include "Core/Job.hh"
#include "IO/BufferStream.hh"
#include "IO/File.hh"
#include "Resource/Resource.hh"

namespace lr::Resource
{
namespace Parser
{
    bool ParseGLTF(BufferReadStream fileData, BufferWriteStream &data, eastl::string_view workingDir);
    bool ParseGLSL(BufferReadStream fileData, ShaderResource &resource, eastl::string_view workingDir);
}
}  // namespace lr::Resource