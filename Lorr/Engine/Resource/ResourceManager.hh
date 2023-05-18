// Created on Monday May 15th 2023 by exdal
// Last modified on Thursday May 18th 2023 by exdal

#pragma once

#include "Resource.hh"

#include "Core/SpinLock.hh"
#include "Memory/Allocator/TLSFAllocator.hh"

namespace lr::Resource
{
struct ResourceManager
{
    void Init(eastl::string_view metaPath);

    template<typename _Resource>
    _Resource *Get(eastl::string_view name)
    {
        ZoneScoped;

        auto FindInMap = [](eastl::string_view name, auto &map)
        {
            auto iterator = map.find(name);
            if (iterator != map.end())
                return iterator->second;

            return nullptr;
        };

        if constexpr (eastl::is_same<_Resource, ShaderResource>())
            return FindInMap(name, m_Shaders);
    }

    template<typename _Resource>
    _Resource *Add(eastl::string_view name, _Resource &resource)
    {
        ScopedSpinLock _(m_AllocatorLock);

        void *pBlock = nullptr;
        u8 *pResourceData = (u8 *)m_Allocator.Allocate(sizeof(_Resource) + PTR_SIZE, 1, &pBlock);
        if (!pResourceData)
        {
            LOG_ERROR("Failed to allocate space for Resource<{}>. Out of memory.", _Resource::type);
            return nullptr;
        }

        _Resource *pResource = new (pResourceData + PTR_SIZE) _Resource;

        memcpy(pResourceData, &pBlock, PTR_SIZE);
        memcpy(pResource, &resource, sizeof(_Resource));

        if constexpr (eastl::is_same<_Resource, ShaderResource>())
            m_Shaders[name] = pResource;

        return pResource;
    }

    template<typename _Resource>
    void Delete(eastl::string_view name)
    {
        ScopedSpinLock _(m_AllocatorLock);

        _Resource *pResource = Get<_Resource>(name);
        Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)(((u8 *)pResource) - PTR_SIZE);
        m_Allocator.Free(pBlock, false);

        // Let's keep the data inside map, it's a pointer anyway, not a big deal to hold
    }

    eastl::unordered_map<eastl::string_view, ShaderResource *> m_Shaders;

    Memory::TLSFAllocator m_Allocator;
    SpinLock m_AllocatorLock;
};
}  // namespace lr::Resource
