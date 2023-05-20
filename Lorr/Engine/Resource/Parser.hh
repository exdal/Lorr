// Created on Thursday May 18th 2023 by exdal
// Last modified on Saturday May 20th 2023 by exdal

#pragma once

#include "Core/Job.hh"
#include "IO/BufferStream.hh"
#include "IO/File.hh"
#include "Resource/Resource.hh"

namespace lr::Resource
{
namespace Parser
{
    bool ParseGLTF(BufferReadStream fileData, BufferWriteStream &data);
    bool ParseGLSL(BufferReadStream fileData, ShaderResource &resource);
}
}  // namespace lr::Resource