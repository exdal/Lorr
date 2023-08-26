// Created on Thursday May 18th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

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
bool Parser::ParseGLSL(eastl::string_view code, ShaderResource &resource, eastl::string_view workingDir)
{
    ZoneScoped;

    Graphics::ShaderStage stage;
    Graphics::ShaderFlag flags = Graphics::ShaderFlag::GenerateDebugInfo;
    eastl::string processedCode;
    processedCode.reserve(code.length());

    eastl::string_view line;
    while (ls::GetLine(code, line))
    {
        switch (lr::Hash::FNV64String(line))
        {
            case LR_SHADER_PRAGMA_EXTENSIONS:
                processedCode += "#extension GL_ARB_gpu_shader_int64 : require\n";
                processedCode += "#extension GL_EXT_buffer_reference : enable\n";
                processedCode += "#extension GL_EXT_buffer_reference2 : enable\n";
                processedCode += "#extension GL_EXT_shader_image_int64 : require\n";
                processedCode += "#extension GL_GOOGLE_include_directive : enable\n";
                processedCode += "#extension GL_EXT_shader_atomic_int64 : require\n";
                processedCode += "#extension GL_EXT_scalar_block_layout : require\n";
                processedCode += "#extension GL_EXT_nonuniform_qualifier : require\n";
                processedCode += "#extension GL_EXT_samplerless_texture_functions : require\n";
                processedCode += "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n";
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
        .m_WorkingDir = workingDir,
        .m_Code = processedCode,
        .m_Type = stage,
        .m_Flags = flags,
    };

    resource.get() = Graphics::ShaderCompiler::CompileShader(&compileDesc);

    return resource.get().IsValid();
}
}  // namespace lr::Resource