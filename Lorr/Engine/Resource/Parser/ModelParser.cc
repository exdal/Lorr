// Created on Saturday April 22nd 2023 by exdal
// Last modified on Thursday May 18th 2023 by exdal

#include "Resource/Parser.hh"

#include <tiny_gltf.h>

namespace lr::Resource
{
bool Parser::ParseGLTF(BufferReadStream fileData, BufferWriteStream &data)
{
    tinygltf::Model model = {};
    tinygltf::TinyGLTF loader = {};
    std::string error;
    std::string warning;

    if (!loader.LoadBinaryFromMemory(&model, &error, &warning, fileData.GetData(), fileData.GetLength()))
    {
        LOG_ERROR("Cannot load GLTF Model! Error: {}. Warning: {}", error, warning);
        return false;
    }

    return true;
}
}  // namespace lr::Resource