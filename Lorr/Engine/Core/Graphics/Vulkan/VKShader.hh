//
// Created on Friday 28th October 2022 by exdal
//

#pragma once

#include "Core/IO/BufferStream.hh"

#include "Allocator.hh"
#include "Vulkan.hh"

namespace lr::Graphics
{
enum ShaderStage : u32
{
    LR_SHADER_STAGE_NONE = 0,
    LR_SHADER_STAGE_VERTEX = 1 << 0,
    LR_SHADER_STAGE_PIXEL = 1 << 1,
    LR_SHADER_STAGE_COMPUTE = 1 << 2,
    LR_SHADER_STAGE_HULL = 1 << 3,
    LR_SHADER_STAGE_DOMAIN = 1 << 4,

    LR_SHADER_STAGE_COUNT = 5,
};

EnumFlags(ShaderStage);

enum ShaderFlags : u32
{
    LR_SHADER_FLAG_NONE = 0,
    LR_SHADER_FLAG_GENERATE_DEBUG_INFO = 1 << 1,
    LR_SHADER_FLAG_SKIP_OPTIMIZATION = 1 << 2,
    LR_SHADER_FLAG_SKIP_VALIDATION = 1 << 3,
};
EnumFlags(ShaderFlags);

struct Shader : APIObject<VK_OBJECT_TYPE_SHADER_MODULE>
{
    ShaderStage m_Type = LR_SHADER_STAGE_NONE;
    VkShaderModule m_pHandle;
};

struct ShaderCompileDesc
{
    static constexpr u32 kMaxDefineCount = 16;

    eastl::string_view m_PathOpt;

    ShaderStage m_Type = LR_SHADER_STAGE_NONE;
    ShaderFlags m_Flags = LR_SHADER_FLAG_NONE;

    u32 m_DefineCount = 0;
    struct
    {
        eastl::string_view m_Name;
        eastl::string_view m_Value;
    } m_Defines[kMaxDefineCount] = {};
};

namespace ShaderCompiler
{
    void Init();
    bool CompileShader(ShaderCompileDesc *pDesc, BufferReadStream &buffer, u32 *&pDataOut, u32 &dataSizeOut);
}  // namespace ShaderCompiler

}  // namespace lr::Graphics