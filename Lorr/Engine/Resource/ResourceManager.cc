// Created on Monday May 15th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

#include "ResourceManager.hh"
#include "Resource/Parser.hh"

#include "Core/Config.hh"
#include "LM/Parser.hh"
#include "STL/String.hh"

namespace lr::Resource
{
void ResourceManager::Init()
{
    ZoneScoped;

    LOG_TRACE("Initializing resources...");

    m_Allocator.Init({
        .m_DataSize = CONFIG_GET_VAR(rm_memory_mb),
        .m_BlockCount = CONFIG_GET_VAR(rm_max_allocs),
    });

    Graphics::ShaderCompiler::Init();

    eastl::string_view workingDir = CONFIG_GET_VAR(resource_meta_dir);

    lm::Result result = {};
    lm::Init(&result);
    if (!lm::ParseFromFile(&result, _FMT("{}/{}", workingDir, CONFIG_GET_VAR(resource_meta_file))))
    {
        LOG_CRITICAL("ResourceMeta file not found!");
        return;
    }

    /// META ///
    lm::Category &meta = result.GetCategory("Meta");

    /// SHADERS ///
    LOG_TRACE("Loading Shaders...");

    lm::Category &shaders = result.GetCategory("Meta.Shaders");
    eastl::span<char *> shaderFiles;
    shaders.GetArray<char *>("Files", shaderFiles);

    for (char *&pShaderPath : shaderFiles)
    {
        eastl::string resourceName = _FMT("shader://{}", ls::TrimFileName(pShaderPath));
        LOG_TRACE("Compiling shader '{}'...", resourceName);

        FileView file(_FMT("{}/{}", workingDir, pShaderPath));
        eastl::string code;
        file.Read(code, file.Size());

        ShaderResource outResource = {};
        if (!Parser::ParseGLSL(code, outResource, workingDir))
        {
            return;
        }

        LOG_TRACE("Compiled shader '{}', SPIR-V module: '{}'", resourceName, outResource.get().m_pShader);

        Add(resourceName, outResource);
    }
}

}  // namespace lr::Resource