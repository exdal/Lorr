#include "Shader.hh"

#include <slang-com-ptr.h>
#include <slang.h>

namespace lr::graphics {
struct CompilerContext {
    Slang::ComPtr<slang::IGlobalSession> global_session;
} compiler_ctx;

std::vector<slang::CompilerOptionEntry> get_slang_entries(ShaderCompileFlag flags)
{
    // clang-format off
    std::vector<slang::CompilerOptionEntry> entries = {};
    entries.emplace_back(
        slang::CompilerOptionName::VulkanUseEntryPointName, 
        slang::CompilerOptionValue{ .intValue0 = true });

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
            slang::CompilerOptionName::MatrixLayoutRow,
            slang::CompilerOptionValue{ .intValue0 = true });
    }
    else if (flags & ShaderCompileFlag::MatrixColumnMajor) {
        entries.emplace_back(
            slang::CompilerOptionName::MatrixLayoutColumn,
            slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::InvertY) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanInvertY,
            slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::DXPositionW) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanUseDxPositionW,
            slang::CompilerOptionValue{ .intValue0 = true });
    }

    if (flags & ShaderCompileFlag::UseGLLayout) {
        entries.emplace_back(
            slang::CompilerOptionName::VulkanUseGLLayout,
            slang::CompilerOptionValue{ .intValue0 = true });
    }
    else if (flags & ShaderCompileFlag::UseScalarLayout) {
        entries.emplace_back(
            slang::CompilerOptionName::GLSLForceScalarLayout,
            slang::CompilerOptionValue{ .intValue0 = true });
    }

    // clang-format on

    return std::move(entries);
}

bool ShaderCompiler::init()
{
    ZoneScoped;

    slang::createGlobalSession(compiler_ctx.global_session.writeRef());
    return true;
}

Result<std::vector<u32>, VKResult> ShaderCompiler::compile(const ShaderCompileInfo &info)
{
    ZoneScoped;

    /////////////////////////////////////////
    /// SESSION INITIALIZATION

    std::vector<slang::CompilerOptionEntry> entries = get_slang_entries(info.compile_flags);
    std::vector<slang::PreprocessorMacroDesc> macros;
    for (ShaderPreprocessorMacroInfo &v : info.definitions) {
        macros.emplace_back(v.name.data(), v.value.data());
    }

    std::vector<const char *> include_dirs;
    for (std::string_view dir : info.include_dirs) {
        include_dirs.push_back(dir.data());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = compiler_ctx.global_session->findProfile("spirv_1_4"),
        .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
        .compilerOptionEntries = entries.data(),
        .compilerOptionEntryCount = static_cast<u32>(entries.size()),
    };

    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .searchPaths = include_dirs.data(),
        .searchPathCount = static_cast<u32>(include_dirs.size()),
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = static_cast<u32>(macros.size()),
    };
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(compiler_ctx.global_session->createSession(session_desc, session.writeRef()))) {
        LR_LOG_ERROR("Failed to create compiler session!");
        return VKResult::Unknown;
    }

    /////////////////////////////////////////
    /////////////////////////////////////////

    // https://github.com/shader-slang/slang/blob/ed0681164d78591148781d08934676bfec63f9da/examples/cpu-com-example/main.cpp
    Slang::ComPtr<SlangCompileRequest> compile_request;
    if (SLANG_FAILED(session->createCompileRequest(compile_request.writeRef()))) {
        LR_LOG_ERROR("Failed to create compile request!");
        return VKResult::Unknown;
    }

    for (auto &vdir : info.virtual_env) {
        for (const auto &[vfile_path, vfile] : vdir.files()) {
            i32 file_id = compile_request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, vfile_path.c_str());
            const char *file_data_begin = reinterpret_cast<const char *>(vfile.data());
            const char *file_data_end = reinterpret_cast<const char *>(vfile.data() + vfile.size());

            compile_request->addTranslationUnitSourceStringSpan(file_id, vfile_path.c_str(), file_data_begin, file_data_end);
        }
    }

    i32 main_shader_id = compile_request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, nullptr);
    const char *source_data = info.code.data();
    const char *source_path = info.real_path.data();
    compile_request->addTranslationUnitSourceStringSpan(main_shader_id, source_path, source_data, source_data + info.code.length());
    const SlangResult compile_result = compile_request->compile();
    const char *diagnostics = compile_request->getDiagnosticOutput();
    if (SLANG_FAILED(compile_result)) {
        LR_LOG_ERROR("Failed to compile shader!");
        LR_LOG_ERROR("{}", diagnostics);

        return VKResult::Unknown;
    }

    Slang::ComPtr<slang::IModule> shader_module;
    if (SLANG_FAILED(compile_request->getModule(main_shader_id, shader_module.writeRef()))) {
        // if this gets hit, something is def wrong
        LR_LOG_ERROR("Failed to get shader module!");
        return VKResult::Unknown;
    }

    /////////////////////////////////////////
    /// REFLECTION

    SlangReflection *reflection = compile_request->getReflection();
    u32 entry_point_count = spReflection_getEntryPointCount(reflection);
    u32 found_entry_point_id = ~0u;
    for (u32 i = 0; i < entry_point_count; i++) {
        SlangReflectionEntryPoint *entry_point = spReflection_getEntryPointByIndex(reflection, i);
        std::string_view entry_point_name = spReflectionEntryPoint_getName(entry_point);
        if (entry_point_name == info.entry_point) {
            found_entry_point_id = i;
            break;
        }
    }

    if (found_entry_point_id == ~0u) {
        LR_LOG_ERROR("Failed to find given entry point ''!", info.entry_point);
        return VKResult::Unknown;
    }

    /////////////////////////////////////////
    /// RESULT BLOB/CLEAN UP

    Slang::ComPtr<slang::IBlob> spirv_blob;
    if (SLANG_FAILED(compile_request->getEntryPointCodeBlob(found_entry_point_id, 0, spirv_blob.writeRef()))) {
        LR_LOG_ERROR("Failed to get entrypoint assembly!");
        return VKResult::Unknown;
    }

    std::span<const u32> spirv_data_view(static_cast<const u32 *>(spirv_blob->getBufferPointer()), spirv_blob->getBufferSize() / sizeof(u32));
    std::vector<u32> spirv_data(spirv_data_view.begin(), spirv_data_view.end());
    return std::move(spirv_data);
}

}  // namespace lr::graphics
