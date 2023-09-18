#pragma once

#include "Crypt/xx.hh"

namespace lr::Resource
{
// Resource Identifiers
//
//
struct Identifier
{
    using Hash_t = u64;
    constexpr static eastl::string_view kSplitter = "://";

    constexpr Identifier() = default;
    constexpr Identifier(eastl::string_view name)
    {
        // Note: Using non-simd hashing function for potential compiler opt.
        usize splitterPos = name.find(kSplitter);
        m_CategoryHash = Hash::XXH3String(name.substr(0, splitterPos));
        m_NameHash = Hash::XXH3String(name.substr(splitterPos));
    }

    constexpr bool IsSameCategory(const Identifier &other)
    {
        return other.m_CategoryHash == m_CategoryHash;
    }

    Hash_t m_CategoryHash = ~0;
    Hash_t m_NameHash = ~0;
};

inline constexpr bool operator==(const Identifier &lhs, const Identifier &rhs)
{
    return lhs.m_CategoryHash == rhs.m_CategoryHash
           && lhs.m_NameHash == rhs.m_CategoryHash;
}

}  // namespace lr::Resource