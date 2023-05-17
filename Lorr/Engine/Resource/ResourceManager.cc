// Created on Monday May 15th 2023 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#include "ResourceManager.hh"

#include "Config.hh"

namespace lr::Resource
{
void ResourceManager::Init(eastl::string_view metaPath)
{
    ZoneScoped;

    m_Allocator.Init({
        .m_DataSize = CONFIG_GET_VAR(rm_memory_mb),
        .m_BlockCount = CONFIG_GET_VAR(rm_max_allocs),
    });
}

}  // namespace lr::Resource