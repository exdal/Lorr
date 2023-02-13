//
// Created on Saturday 6th August 2022 by e-erdal
//

#pragma once

#include "Light.hh"

namespace lr::Hash
{
    template<typename T>
    constexpr u64 GetTypeHash()
    {
#ifdef _MSC_VER
        return FNV64HashOf(__FUNCSIG__);
#else
        return FNV64HashOf(__PRETTY_FUNCTION__);
#endif
    }

}  // namespace lr::Hash