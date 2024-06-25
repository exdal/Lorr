#pragma once

#define LR_TYPEOP_ARITHMETIC(lhsType, rhsType, op)                                                      \
    constexpr lhsType operator op(const lhsType &lhs, const rhsType &rhs)                               \
    {                                                                                                   \
        return (lhsType)(std::underlying_type_t<lhsType>(lhs) op std::underlying_type_t<rhsType>(rhs)); \
    }

#define LR_TYPEOP_ASSIGNMENT(lhsType, rhsType, op)                                                            \
    constexpr lhsType &operator op##=(lhsType & lhs, const rhsType rhs)                                       \
    {                                                                                                         \
        return lhs = (lhsType)(std::underlying_type_t<lhsType>(lhs) op std::underlying_type_t<rhsType>(rhs)); \
    }

#define LR_TYPEOP_ARITHMETIC_INT(lhsType, rhsType, op)                                              \
    constexpr std::underlying_type_t<lhsType> operator op(const lhsType & lhs, const rhsType & rhs) \
    {                                                                                               \
        return (std::underlying_type_t<lhsType>(lhs) op std::underlying_type_t<rhsType>(rhs));      \
    }
