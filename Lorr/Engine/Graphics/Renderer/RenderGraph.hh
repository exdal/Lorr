// Created on Friday February 24th 2023 by exdal
// Last modified on Tuesday July 18th 2023 by exdal

#pragma once

#include "Graphics/APIContext.hh"

namespace lr::Graphics
{
struct RenderGraph
{
    void Init(APIContext *pContext);

    void Prepare();

    APIContext *m_pContext = nullptr;
};

}  // namespace lr::Graphics