#include "Shader.hh"

#include <slang-com-ptr.h>
#include <slang.h>

namespace lr::graphics {
struct CompilerContext {
    Slang::ComPtr<slang::IGlobalSession> global_session;
} compiler_ctx;

std::vector<slang::CompilerOptionEntry> get_slang_entries(ShaderCompileFlag flags)
{
    std::vector<slang::CompilerOptionEntry> entries = {};
    entries.emplace_back(
        slang::CompilerOptionName::VulkanUseEntryPointName, slang::CompilerOptionValue{ .intValue0 = true });

    if (flags & ShaderCompileFlag::GenerateDebugInfo) {
        entries.emplace_back(
            slang::CompilerOptionName::DebugInformation,
            slang::CompilerOptionValue({ .intValue0 = SLANG_DEBUG_INFO_FORMAT_PDB }));
    }

    if (flags & ShaderCompileFlag::SkipOptimization) {
        entries.emplace_back(
            slang::CompilerOptionName::Optimization,
            slang::CompilerOptionValue{ .intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE });
    }
    else {
        entries.emplace_back(
            slang::CompilerOptionName::Optimization,
            slang::CompilerOptionValue{ .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL });
    }

    if (flags & ShaderCompileFlag::SkipValidation) {
        entries.emplace_back(
            slang::CompilerOptionName::SkipSPIRVValidation,
            slang::CompilerOptionValue({ .intValue0 = true }));
    }

    if (flags & ShaderCompileFlag::MatrixRowMajor) {
        entries.emplace_back(
            slang::CompilerOptionName::MatrixLayoutRow, slang::CompilerOptionValue{ .intValue0 = true });
    }
    else if (flags & ShaderCompileFlag::MatrixColumnMajor) {
        entries.emplace_back(
            slang::CompilerOptionName::MatrixLayoutColumn, slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::InvertY) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanInvertY, slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::DXPositionW) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanUseDxPositionW, slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::UseGLLayout) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanUseGLLayout, slang::CompilerOptionValue{ .intValue0 = true });
    }
    else if (flags & ShaderCompileFlag::UseScalarLayout) {
        entries.emplace_back(
            slang::CompilerOptionName::GLSLForceScalarLayout,
            slang::CompilerOptionValue{ .intValue0 = true });
    }

    return std::move(entries);
}

bool ShaderCompiler::init()
{
    slang::createGlobalSession(compiler_ctx.global_session.writeRef());
    return true;
}

Result<std::vector<u32>, VKResult> ShaderCompiler::compile_shader(const ShaderCompileInfo &info)
{
    ZoneScoped;

    std::vector<slang::PreprocessorMacroDesc> macros;
    for (ShaderPreprocessorMacroInfo &v : info.definitions) {
        macros.emplace_back(v.name.data(), v.value.data());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = compiler_ctx.global_session->findProfile("spirv_1_4"),
        .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
        //   .compilerOptionEntries = entries.data(),
        //   .compilerOptionEntryCount = static_cast<u32>(entries.size()),
    };

    std::vector<const char *> include_dirs;
    // for (std::string_view dir : info.include_dirs) {
    //     include_dirs.push_back(dir.data());
    // }

    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .searchPaths = include_dirs.data(),
        .searchPathCount = static_cast<u32>(include_dirs.size()),
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = static_cast<u32>(macros.size()),
    };
    Slang::ComPtr<slang::ISession> session;
    compiler_ctx.global_session->createSession(session_desc, session.writeRef());

    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    slang::IModule *module = session->loadModule(info.file_path.data(), diagnostics_blob.writeRef());
    if (diagnostics_blob != nullptr) {
        LR_LOG_ERROR(
            "Shader error: {}", reinterpret_cast<const char *>(diagnostics_blob->getBufferPointer()));
    }

    if (!module) {
        LR_LOG_ERROR("Failed to load shader module!");
        return VKResult::Unknown;
    }

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    module->findEntryPointByName(info.entry_point.data(), entry_point.writeRef());

    return {};
}

/*
ShaderReflectionData ShaderCompiler::reflect_spirv(ShaderReflectionDesc *desc,
eastl::span<u32> ir)
{
    ZoneScoped;

    constexpr static ShaderStage kStageMap[] = {
        [spv::ExecutionModel::ExecutionModelVertex] = ShaderStage::Vertex,
        [spv::ExecutionModel::ExecutionModelTessellationControl] =
ShaderStage::TessellationControl,
        [spv::ExecutionModel::ExecutionModelTessellationEvaluation] =
ShaderStage::TessellationEvaluation, [spv::ExecutionModel::ExecutionModelFragment] =
ShaderStage::Pixel, [spv::ExecutionModel::ExecutionModelGLCompute] = ShaderStage::Compute,
    };

    spirv_cross::CompilerHLSL compiler(ir.data(), ir.size());
    auto first_entry = compiler.get_entry_points_and_stages()[0];
    auto resources = compiler.get_shader_resources();
    auto &entry_point = compiler.get_entry_point(first_entry.name,
first_entry.execution_model);

    ShaderReflectionData result = {};
    result.m_compiled_stage = kStageMap[entry_point.model];
    result.m_entry_point = first_entry.name.data();

    for (const auto &push_constant_buffer : resources.push_constant_buffers)
    {
        auto &type = compiler.get_type(push_constant_buffer.base_type_id);
        for (u32 i = 0; i < type.member_types.size(); i++)
        {
            usize size = compiler.get_declared_struct_member_size(type, i);
            usize offset = compiler.type_struct_member_offset(type, i);

            for (const auto &type_member : type.member_types)
            {
                const auto &name = compiler.get_name(type_member);
                if (desc->m_descriptors_struct_name == eastl::string_view(name.data(),
name.size()))
                {
                    result.m_descriptors_struct.offset = offset;
                    result.m_descriptors_struct.size = size;
                }
            }

            result.m_push_constant_size += size;
        }
    }

    return result;
}
*/
}  // namespace lr::graphics
