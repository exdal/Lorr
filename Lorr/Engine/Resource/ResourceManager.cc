// Created on Monday May 15th 2023 by exdal
// Last modified on Saturday May 20th 2023 by exdal

#include "ResourceManager.hh"
#include "Resource/Parser.hh"

#include "Core/Config.hh"
#include "LM/Parser.hh"

namespace lr::Resource
{
void ResourceManager::Init()
{
    ZoneScoped;

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
    lm::Category &shaders = result.GetCategory("Meta.Shaders");
    eastl::span<char *> shaderVals;
    shaders.GetArray<char *>("Files", shaderVals);

    for (char *pShaderPath : shaderVals)
    {
        FileStream fs(Format("{}/{}", workingDir, pShaderPath), false);
        const char *pScript = fs.ReadAll<char>();
        u32 size = fs.Size();
        fs.Close();

        ShaderResource outResource = {};
        Parser::ParseGLSL(BufferReadStream((u8 *)pScript, size), outResource);
    }
}

}  // namespace lr::Resource