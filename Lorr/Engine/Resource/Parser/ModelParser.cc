// Created on Saturday April 22nd 2023 by exdal
// Last modified on Wednesday August 2nd 2023 by exdal

#include "Resource/Parser.hh"

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>

namespace lr::Resource
{
bool Parser::ParseGLTF(BufferReadStream fileData, BufferWriteStream &data, eastl::string_view workingDir)
{
    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer gltfBuffer;
    gltfBuffer.copyBytes(fileData.GetData(), fileData.GetLength());

    auto gltf = parser.loadGLTF(
        &gltfBuffer, std::string_view(workingDir.data(), workingDir.length()), fastgltf::Options::None);

    return true;
}
}  // namespace lr::Resource