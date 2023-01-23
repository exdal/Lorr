//
// Created on Saturday 6th August 2022 by e-erdal
//

#pragma once

#include "Light.hh"

namespace lr::c
{
    struct TypeHash
    {
        template<typename T>
        void Push(T &&type)
        {
            m_pData = (u8 *)realloc(m_pData, m_DataLen + sizeof(type));
            memcpy(m_pData + m_DataLen, &type, sizeof(type));

            m_DataLen += sizeof(type);
        }

        void Push(void *pVal, u32 size)
        {
            m_pData = (u8 *)realloc(m_pData, m_DataLen + size);
            memcpy(m_pData + m_DataLen, pVal, size);

            m_DataLen += size;
        }

        u32 Get()
        {
            return FNVHash((const char *)m_pData, m_DataLen);
        }

        u32 m_DataLen = 0;
        u8 *m_pData = nullptr;
    };

}  // namespace lr::c