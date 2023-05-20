// Created on Monday May 15th 2023 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#include "ResourceManager.hh"
#include "Resource/Parser.hh"

#include "Core/Config.hh"
#include "LM/Parser.hh"

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
    if (!lm::ParseFromFile(&result, Format("{}/{}", workingDir, CONFIG_GET_VAR(resource_meta_file))))
    {
        LOG_CRITICAL("ResourceMeta file not found!");
        return;
    }

    /// META ///
    lm::Category &meta = result.GetCategory("Meta");

    /// SHADERS ///
    LOG_TRACE("Loading Shaders...");

    lm::Category &shaders = result.GetCategory("Meta.Shaders");
    eastl::span<char *> shaderVals;
    shaders.GetArray<char *>("Files", shaderVals);

    u32 shaderID = 0;
    for (char *&pShaderPath : shaderVals)
    {
        Job::JobManager::Schedule(
            [=]()
            {
                LOG_TRACE("Compiling shader '{}'...", pShaderPath);
                FileView file(Format("{}/{}", workingDir, pShaderPath));
                ShaderResource outResource = {};
                Parser::ParseGLSL(BufferReadStream(file), outResource);
                LOG_TRACE(
                    "Compiled shader '{}', SPIR-V module: '{}'", pShaderPath, outResource.get().m_pShader);
            });
    }

    Job::JobManager::WaitForAll();
}

}  // namespace lr::Resource