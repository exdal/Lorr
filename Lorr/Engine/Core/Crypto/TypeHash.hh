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
            pData = (u8 *)realloc(pData, DataLen + sizeof(type));
            memcpy(pData + DataLen, &type, sizeof(type));

            DataLen += sizeof(type);
        }

        void Push(void *pVal, u32 size)
        {
            pData = (u8 *)realloc(pData, DataLen + size);
            memcpy(pData + DataLen, pVal, size);

            DataLen += size;
        }

        u32 Get()
        {
            return FNVHash((const char *)pData, DataLen);
        }

        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

}  // namespace lr::c