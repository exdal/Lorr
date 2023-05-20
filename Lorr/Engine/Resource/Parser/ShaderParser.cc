// Created on Thursday May 18th 2023 by exdal
// Last modified on Saturday May 20th 2023 by exdal

#include "Resource/Parser.hh"

#include "Crypt/FNV.hh"
#include "Graphics/Shader.hh"
#include "STL/String.hh"

#define LR_SHADER_PRAGMA(name) FNV64HashOf("#pragma $" name)
#define LR_SHADER_PRAGMA_EXTENSIONS LR_SHADER_PRAGMA("extensions")
#define LR_SHADER_PRAGMA_STAGE_VERTEX LR_SHADER_PRAGMA("stage_vertex")
#define LR_SHADER_PRAGMA_STAGE_FRAGMENT LR_SHADER_PRAGMA("stage_fragment")
#define LR_SHADER_PRAGMA_STAGE_COMPUTE LR_SHADER_PRAGMA("stage_compute")

namespace lr::Resource
{
bool Parser::ParseGLSL(BufferReadStream fileData, ShaderResource &resource)
{
    ZoneScoped;

    Graphics::ShaderStage stage;
    Graphics::ShaderFlag flags = Graphics::ShaderFlag::None;
    eastl::string_view code((const char *)fileData.GetData(), fileData.GetLength());
    eastl::string processedCode;
    processedCode.reserve(code.length());

    eastl::string_view line;
    while (ls::GetLine(code, line))
    {
        switch (lr::Hash::FNV64String(line))
        {
            case LR_SHADER_PRAGMA_EXTENSIONS:
                break;

            case LR_SHADER_PRAGMA_STAGE_VERTEX:
                stage = Graphics::ShaderStage::Vertex;
                break;

            case LR_SHADER_PRAGMA_STAGE_FRAGMENT:
                stage = Graphics::ShaderStage::Pixel;
                break;

            case LR_SHADER_PRAGMA_STAGE_COMPUTE:
                stage = Graphics::ShaderStage::Compute;
                break;

            default:
                processedCode.append(line.data(), line.length());
                processedCode += '\n';
                break;
        }
    }

    Graphics::ShaderCompileDesc compileDesc = {
        .m_Code = processedCode,
        .m_Type = stage,
        .m_Flags = flags,
    };

    resource.get() = Graphics::ShaderCompiler::CompileShader(&compileDesc);

    return true;
}
}  // namespace lr::Resource