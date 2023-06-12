// Created on Saturday April 22nd 2023 by exdal
// Last modified on Monday June 12th 2023 by exdal

#include "Resource/Parser.hh"

namespace lr::Resource
{
bool Parser::ParseGLTF(BufferReadStream fileData, BufferWriteStream &data, eastl::string_view workingDir)
{
    // tinygltf::Model model = {};
    // tinygltf::TinyGLTF loader = {};
    // std::string error;
    // std::string warning;

    // if (!loader.LoadBinaryFromMemory(&model, &error, &warning, fileData.GetData(), fileData.GetLength()))
    // {
    //     LOG_ERROR("Cannot load GLTF Model! Error: {}. Warning: {}", error, warning);
    //     return false;
    // }

    return true;
}
}  // namespace lr::Resource