// Created on Saturday April 22nd 2023 by exdal
// Last modified on Wednesday June 28th 2023 by exdal

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

    if (parser.getError() != fastgltf::Error::None)
    {
        LOG_ERROR("Failed to load GLTF! Parser error: {}", (u32)parser.getError());
        return false;
    }

    fastgltf::Error parserResult = gltf->parse();
    if (parserResult != fastgltf::Error::None)
    {
        LOG_ERROR("Failed to parse GLTF! Parser error: {}", (u32)parser.getError());
        return false;
    }

    std::unique_ptr<fastgltf::Asset> asset = gltf->getParsedAsset();

    return true;
}
}  // namespace lr::Resource