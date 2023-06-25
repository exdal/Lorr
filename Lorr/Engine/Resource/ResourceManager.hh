// Created on Monday May 15th 2023 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#pragma once

#include "Resource.hh"

#include "Core/SpinLock.hh"
#include "Crypt/FNV.hh"
#include "Memory/Allocator/TLSFAllocator.hh"

namespace lr::Resource
{
struct ResourceManager
{
    void Init();

    template<typename _Resource>
    _Resource *Get(eastl::string_view name)
    {
        ZoneScoped;

        u64 hash = Hash::FNV64String(name);

        for (auto &[resourceHash, pPtr] : m_Resources)
        {
            if (resourceHash == hash)
                return (_Resource *)pPtr;
        }

        return (_Resource *)nullptr;
    }

    template<typename _Resource>
    _Resource *Add(eastl::string_view name, _Resource &resource)
    {
        ZoneScoped;

        ScopedSpinLock _(m_AllocatorLock);

        void *pBlock = nullptr;
        u8 *pResourceData =
            (u8 *)m_Allocator.Allocate(sizeof(_Resource) + Memory::TLSFAllocatorView::ALIGN_SIZE, 1, &pBlock);
        if (!pResourceData)
        {
            LOG_ERROR("Failed to allocate space for Resource<{}>. Out of memory.", (u32)resource.m_Type);
            return nullptr;
        }

        _Resource *pResource = new (pResourceData + Memory::TLSFAllocatorView::ALIGN_SIZE) _Resource;

        memcpy(pResourceData, &pBlock, Memory::TLSFAllocatorView::ALIGN_SIZE);
        memcpy(pResource, &resource, sizeof(_Resource));

        m_Resources.push_back(eastl::make_pair(Hash::FNV64String(name), (void *)pResource));

        return pResource;
    }

    template<typename _Resource>
    void Delete(eastl::string_view name)
    {
        ZoneScoped;

        ScopedSpinLock _(m_AllocatorLock);

        _Resource *pResource = Get<_Resource>(name);
        ResourceWrapper<_Resource>::Destroy(*pResource);

        Memory::TLSFBlockID blockID =
            *(Memory::TLSFBlockID *)((u8 *)pResource - Memory::TLSFAllocatorView::ALIGN_SIZE);
        m_Allocator.Free(blockID, false);

        // Let's keep the data inside map, it's a pointer anyway, not a big deal to hold
    }

    eastl::vector<eastl::pair<u64, void *>> m_Resources;

    Memory::TLSFAllocator m_Allocator;
    SpinLock m_AllocatorLock;
};
}  // namespace lr::Resource
