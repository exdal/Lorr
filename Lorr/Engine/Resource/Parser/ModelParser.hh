//
// Created on Saturday 22nd April 2023 by exdal
//

#pragma once

#include "IO/BufferStream.hh"

namespace lr::Resource
{
namespace ModelParser
{
    bool ParseGLTF(BufferReadStream fileData, BufferWriteStream &data);
}

}  // namespace lr::Resource