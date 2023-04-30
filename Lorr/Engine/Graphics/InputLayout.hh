//
// Created on Monday 9th May 2022 by exdal
//

#pragma once

#include "Config.hh"

namespace lr::Graphics
{
enum VertexAttribType : u32
{
    LR_VERTEX_ATTRIB_NONE,
    LR_VERTEX_ATTRIB_SFLOAT,
    LR_VERTEX_ATTRIB_SFLOAT2,
    LR_VERTEX_ATTRIB_SFLOAT3,
    LR_VERTEX_ATTRIB_SFLOAT4,
    LR_VERTEX_ATTRIB_UINT,
    LR_VERTEX_ATTRIB_UINT_4N,
};

constexpr u32 kVertexAttribSizeLUT[] = {
    0,   // LR_VERTEX_ATTRIB_NONE
    4,   // LR_VERTEX_ATTRIB_SFLOAT
    8,   // LR_VERTEX_ATTRIB_SFLOAT2
    12,  // LR_VERTEX_ATTRIB_SFLOAT3
    16,  // LR_VERTEX_ATTRIB_SFLOAT4
    4,   // LR_VERTEX_ATTRIB_UINT
    4,   // LR_VERTEX_ATTRIB_UINT_4N
};

static constexpr u32 GetVertexAttribSize(VertexAttribType type)
{
    return kVertexAttribSizeLUT[type];
}

struct VertexAttrib
{
    constexpr VertexAttrib(){};
    constexpr VertexAttrib(VertexAttribType type) : m_Type(type), m_Size(GetVertexAttribSize(type)){};

    VertexAttribType m_Type : 11;
    u32 m_Size : 11;
    u32 m_Offset : 10;
};

struct InputLayout
{
    constexpr InputLayout(const std::initializer_list<VertexAttrib> &elements)
    {
        m_Count = elements.size();
        m_Stride = 0;

        u32 offset = 0;
        u32 idx = 0;
        for (auto &element : elements)
        {
            VertexAttrib &thisAttrib = m_Elements[idx];

            thisAttrib = element;
            thisAttrib.m_Offset = offset;
            offset += element.m_Size;
            m_Stride += element.m_Size;

            idx++;
        }
    }

    u32 m_Count = 0;
    u32 m_Stride = 0;
    eastl::array<VertexAttrib, LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE> m_Elements = {};
};

}  // namespace lr::Graphics