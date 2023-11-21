// Created on Sunday March 12th 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#include "Shader.hh"

#include <glslang/Include/Common.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/ShaderLang.h>

#include "IO/File.hh"
#include "Memory/MemoryUtils.hh"

static constexpr TBuiltInResource glslang_DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */
    {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};

namespace lr::Graphics
{
constexpr glslang_stage_t to_glsl_stage(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return GLSLANG_STAGE_VERTEX;
        case ShaderStage::Pixel:
            return GLSLANG_STAGE_FRAGMENT;
        case ShaderStage::Compute:
            return GLSLANG_STAGE_COMPUTE;
        case ShaderStage::TessellationControl:
            return GLSLANG_STAGE_TESSCONTROL;
        case ShaderStage::TessellationEvaluation:
            return GLSLANG_STAGE_TESSEVALUATION;
        default:
            break;
    }

    return GLSLANG_STAGE_COUNT;
}
LR_TYPEOP_ARITHMETIC(glslang_messages_t, glslang_messages_t, |);
#define ENABLE_IF_HAS(flag) (bool)(desc->m_flags & flag)

glsl_include_result_t *include_local_cb(void *context, const char *header_name, const char *includer_name, uptr indclude_depth)
{
    ZoneScoped;

    ShaderCompileDesc *compile_desc = static_cast<ShaderCompileDesc *>(context);
    eastl::string fullPath = _FMT("{}/{}", compile_desc->m_working_dir, header_name);
    FileView fv(fullPath);
    if (!fv.IsOK())
    {
        LOG_ERROR("ShaderCompiler: Failed to include file '{}'!", fullPath);
        return nullptr;
    }

    u64 alloc_size = sizeof(glsl_include_result_t) + fv.Size();

    glsl_include_result_t *include_result = Memory::Allocate<glsl_include_result_t>(alloc_size);
    memset(include_result, 0, alloc_size);

    u8 *pCode = (u8 *)include_result + sizeof(glsl_include_result_t);
    fv.Read(pCode, fv.Size());

    include_result->header_name = header_name;
    include_result->header_data = (const char *)pCode;
    include_result->header_length = fv.Size();

    return include_result;
}

glsl_include_result_t *include_system_cb(void *pContext, const char *pHeaderName, const char *pIncluderName, uptr indcludeDepth)
{
    ZoneScoped;

    return include_local_cb(pContext, pHeaderName, pIncluderName, indcludeDepth);
}

int include_result_cb(void *pContext, glsl_include_result_t *pResult)
{
    ZoneScoped;

    free(pResult);

    return 1;
}

void ShaderCompiler::init()
{
    ZoneScoped;

    glslang::InitializeProcess();
}

ShaderCompileOutput ShaderCompiler::compile_shader(ShaderCompileDesc *desc)
{
    ZoneScoped;

    auto PrintError = [](glslang_shader_t *pShader)
    {
        LOG_ERROR(
            "Failed to compile shader!\nINFO: {}\nDEBUG: {}\n=== PROCESSED CODE ===\n{}\n",
            glslang_shader_get_info_log(pShader),
            glslang_shader_get_info_debug_log(pShader),
            glslang_shader_get_preprocessed_code(pShader));

        glslang_shader_delete(pShader);
    };

    ShaderCompileOutput output = {};

    glsl_include_callbacks_t include_callbacks = {
        .include_system = include_system_cb,
        .include_local = include_local_cb,
        .free_include_result = include_result_cb,
    };
    const glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = to_glsl_stage(desc->m_type),
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_3,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_6,
        .code = desc->m_code.data(),
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT,
        .resource = (glslang_resource_t *)&glslang_DefaultTBuiltInResource,
        .callbacks = include_callbacks,
        .callbacks_ctx = desc,
    };

    glslang_shader_t *pShader = glslang_shader_create(&input);
    if (!glslang_shader_preprocess(pShader, &input))
    {
        PrintError(pShader);
        return output;
    }

    if (!glslang_shader_parse(pShader, &input))
    {
        PrintError(pShader);
        return output;
    }

    glslang_program_t *pProgram = glslang_program_create();
    glslang_program_add_shader(pProgram, pShader);

    if (!glslang_program_link(pProgram, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
        PrintError(pShader);
        return output;
    }

    glslang_spv_options_t spvOptions = {
        .generate_debug_info = ENABLE_IF_HAS(ShaderCompileFlag::GenerateDebugInfo),
        .strip_debug_info = !ENABLE_IF_HAS(ShaderCompileFlag::SkipOptimization),
        .disable_optimizer = ENABLE_IF_HAS(ShaderCompileFlag::SkipOptimization),
        .validate = !ENABLE_IF_HAS(ShaderCompileFlag::SkipValidation),
    };
    glslang_program_SPIRV_generate_with_options(pProgram, input.stage, &spvOptions);
    u32 dataSize = glslang_program_SPIRV_get_size(pProgram) * sizeof(u32);
    u32 *pData = glslang_program_SPIRV_get_ptr(pProgram);
    output = {
        .m_stage = desc->m_type,
        .m_data_spv = { pData, dataSize },
        .m_program = pProgram,
        .m_shader = pShader,
    };

    eastl::string_view spvMessage = glslang_program_SPIRV_get_messages(pProgram);
    if (!spvMessage.empty())
        LOG_WARN("Shader Compiler - SPIR-V message: {}", spvMessage);

    return output;
}

void ShaderCompiler::free_program(ShaderCompileOutput &output)
{
    ZoneScoped;

    glslang_program_delete((glslang_program_t *)output.m_program);
    glslang_shader_delete((glslang_shader_t *)output.m_shader);
}

}  // namespace lr::Graphics

#undef ENABLE_IF_HAS