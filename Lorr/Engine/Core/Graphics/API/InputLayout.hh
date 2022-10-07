//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr::g
{
    enum class VertexAttribType
    {
        None = 0,
        Float,
        Vec2,
        Vec3,
        Vec3PK,
        Vec4,
        UInt,
    };

    static constexpr u32 VertexAttribSize(VertexAttribType type)
    {
        switch (type)
        {
            case VertexAttribType::Float: return sizeof(f32);
            case VertexAttribType::Vec2: return sizeof(XMFLOAT2);
            case VertexAttribType::Vec3: return sizeof(XMFLOAT3);
            case VertexAttribType::Vec3PK: return sizeof(XMFLOAT3PK);
            case VertexAttribType::Vec4: return sizeof(XMVECTOR);
            case VertexAttribType::UInt: return sizeof(u32);
            default: return 0;
        }
    }

    struct VertexAttrib
    {
        constexpr VertexAttrib() : m_Type(VertexAttribType::None), m_Name(""), m_Size(VertexAttribSize(VertexAttribType::None))
        {
        }

        constexpr VertexAttrib(VertexAttribType type, eastl::string_view name) : m_Type(type), m_Name(name), m_Size(VertexAttribSize(type))
        {
        }

        eastl::string_view m_Name;
        VertexAttribType m_Type;
        u32 m_Size;
        u32 m_Offset = 0;
    };

    /// Compile time Input Layout because we are cool, problem?
    struct InputLayout
    {
        constexpr InputLayout(const std::initializer_list<VertexAttrib> &elements) : m_Elements({})
        {
            m_Count = elements.size();
            m_Stride = 0;

            size_t offset = 0;
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
        eastl::array<VertexAttrib, 12> m_Elements;
    };

}  // namespace lr