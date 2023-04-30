//
// Created on Saturday 22nd April 2023 by exdal
//

#pragma once

#include "Graphics/Buffer.hh"

namespace lr
{
struct Model
{
    Graphics::Buffer *m_pVertexBuffer = nullptr;
    Graphics::Buffer *m_pIndexBuffer = nullptr;
};

}  // namespace lr
