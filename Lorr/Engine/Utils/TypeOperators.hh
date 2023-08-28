// Created on Saturday August 26th 2023 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

#define LR_TYPEOP_ARITHMETIC(lhsType, rhsType, op)                                                          \
    constexpr lhsType operator op(const lhsType &lhs, const rhsType &rhs)                                   \
    {                                                                                                       \
        return (lhsType)(eastl::underlying_type_t<lhsType>(lhs) op eastl::underlying_type_t<rhsType>(rhs)); \
    }

#define LR_TYPEOP_ARITHMETIC_INT(lhsType, rhsType, op)                                                \
    constexpr eastl::underlying_type_t<lhsType> operator op(const lhsType & lhs, const rhsType & rhs) \
    {                                                                                                 \
        return (eastl::underlying_type_t<lhsType>(lhs) op eastl::underlying_type_t<rhsType>(rhs));    \
    }