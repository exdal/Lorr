// Created on Monday May 15th 2023 by exdal
// Last modified on Friday September 15th 2023 by exdal

#include "ResourceManager.hh"

#include <simdjson.h>

#include "Parser.hh"

#include "Core/Config.hh"
#include "STL/String.hh"

namespace lr::Resource
{
constexpr static u32 kMinVersion = 1;

void ResourceManager::Init()
{
    ZoneScoped;

    LOG_TRACE("Initializing resources...");

    m_Allocator.Init({
        .m_DataSize = CONFIG_GET_VAR(RM_MAX_MEMORY),
        .m_BlockCount = CONFIG_GET_VAR(RM_MAX_ALLOCS),
    });

    Graphics::ShaderCompiler::Init();

    eastl::string_view workingDir = CONFIG_GET_VAR(RESOURCE_META_DIR);
    eastl::string jsonFile =
        _FMT("{}/{}", workingDir, CONFIG_GET_VAR(RESOURCE_META_FILE));

    simdjson::dom::parser parser;
    simdjson::dom::element elements;
    auto error = parser.load(jsonFile.c_str()).get(elements);
    if (error)
    {
        LOG_ERROR(
            "Failed to open resource meta file, {}.", simdjson::error_message(error));
        return;
    }

    /// SHADERS ///
    LOG_TRACE("Loading Shaders...");
    simdjson::dom::array files = elements["shaders"]["files"].get_array();
    for (auto file : files)
    {
        eastl::string_view path(file.get_c_str(), file.get_string_length());
        Identifier id(_FMT("shader://{}", path));

        LOG_TRACE("Shader file: {}", id.m_NameHash);

        // TODO: Better parsing for resources.
    }
}

}  // namespace lr::Resource