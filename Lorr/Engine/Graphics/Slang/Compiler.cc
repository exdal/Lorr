#include <utility>

#include "Engine/Graphics/Slang/Compiler.hh"

#include "Engine/Util/VirtualDir.hh"

namespace lr {
struct SlangBlob : ISlangBlob {
    std::vector<u8> m_data = {};
    std::atomic_uint32_t m_refCount = 1;

    ISlangUnknown *getInterface(SlangUUID const &) { return nullptr; }
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid, void **outObject) SLANG_OVERRIDE {
        ISlangUnknown *intf = getInterface(uuid);
        if (intf) {
            addRef();
            *outObject = intf;
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }

    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return ++m_refCount; }

    SLANG_NO_THROW uint32_t SLANG_MCALL release() override {
        --m_refCount;
        if (m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }

    SlangBlob(const std::vector<u8> &data)
        : m_data(data) {}
    virtual ~SlangBlob() = default;
    SLANG_NO_THROW void const *SLANG_MCALL getBufferPointer() final { return m_data.data(); };
    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() final { return m_data.size(); };
};

// PERF: When we are at Editor environment, shaders obviously needs to be loaded through file system.
// But when we are at production environment, we don't need file system because we probably would
// have proper asset manager with all shaders are preloaded into virtual environment, so ::loadFile
// would just return already existing shader file.
struct SlangVirtualFS : ISlangFileSystem {
    VirtualDir m_virtual_env;
    fs::path m_root_dir;
    std::atomic_uint32_t m_refCount;

    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid, void **outObject) SLANG_OVERRIDE {
        ISlangUnknown *intf = getInterface(uuid);
        if (intf) {
            addRef();
            *outObject = intf;
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }

    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return ++m_refCount; }

    SLANG_NO_THROW uint32_t SLANG_MCALL release() override {
        --m_refCount;
        if (m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }

    SlangVirtualFS(fs::path root_dir)
        : m_virtual_env({}),
          m_root_dir(std::move(root_dir)),
          m_refCount(1) {};
    virtual ~SlangVirtualFS() = default;

    ISlangUnknown *getInterface(SlangUUID const &) { return nullptr; }
    SLANG_NO_THROW void *SLANG_MCALL castAs(const SlangUUID &) final { return nullptr; }

    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const *path_cstr, ISlangBlob **outBlob) final {
        auto path = fs::path(path_cstr);
        if (path.has_extension()) {
            path.replace_extension("slang");
        }

        // /resources/shaders/path/to/xx.slang -> path.to.xx
        auto module_name = fs::relative(path, m_root_dir).replace_extension("").string();
        std::replace(module_name.begin(), module_name.end(), static_cast<c8>(fs::path::preferred_separator), '.');

        auto it = m_virtual_env.files.find(module_name);
        if (it == m_virtual_env.files.end()) {
            if (auto result = m_virtual_env.read_file(module_name, path); result.has_value()) {
                auto &new_file = result.value().get();
                *outBlob = new SlangBlob(std::vector<u8>{ new_file.data(), (new_file.data() + new_file.size()) });

                LOG_TRACE("New shader module '{}' is loaded.", module_name);
                return SLANG_OK;
            } else {
                auto path_str = path.string();
                LOG_ERROR("Failed to load shader '{}'!", path_str);
                return SLANG_E_NOT_FOUND;
            }
        } else {
            auto &file = it->second;
            *outBlob = new SlangBlob(std::vector<u8>{ file.data(), (file.data() + file.size()) });
            return SLANG_OK;
        }
    }
};

std::vector<slang::CompilerOptionEntry> get_slang_entries(ShaderCompileFlag flags) {
    ZoneScoped;

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

    return entries;
}

template<>
struct Handle<SlangModule>::Impl {
    SlangSession *session = nullptr;
    slang::IModule *slang_module = nullptr;
};

template<>
struct Handle<SlangSession>::Impl {
    std::unique_ptr<SlangVirtualFS> shader_virtual_env;
    Slang::ComPtr<slang::ISession> session;
};

template<>
struct Handle<SlangCompiler>::Impl {
    Slang::ComPtr<slang::IGlobalSession> global_session;
};

auto SlangModule::destroy() -> void {
    impl->session->destroy();

    delete impl;
    impl = nullptr;
}

auto SlangModule::get_entry_point(std::string_view name) -> std::vector<u32> {
    ZoneScoped;

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    if (SLANG_FAILED(impl->slang_module->findEntryPointByName(name.data(), entry_point.writeRef()))) {
        LOG_ERROR("Shader entry point '{}' is not found.", name);
        return {};
    }

    std::vector<slang::IComponentType *> component_types;
    component_types.push_back(impl->slang_module);
    component_types.push_back(entry_point);

    Slang::ComPtr<slang::IComponentType> composed_program;
    Slang::ComPtr<slang::IBlob> composite_blob;
    if (SLANG_FAILED(impl->session->impl->session->createCompositeComponentType(
            component_types.data(), component_types.size(), composed_program.writeRef(), composite_blob.writeRef()))) {
        LOG_ERROR("Failed to compose shader program.");
        return {};
    }

    Slang::ComPtr<slang::IBlob> spirv_code;
    Slang::ComPtr<slang::IBlob> entry_point_blob;
    if (SLANG_FAILED(composed_program->getEntryPointCode(0, 0, spirv_code.writeRef(), entry_point_blob.writeRef()))) {
        LOG_ERROR("Failed to compile shader.");
        return {};
    }

    std::vector<u32> output(spirv_code->getBufferSize() / 4);
    std::memcpy(output.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

    return output;
}

auto SlangModule::get_reflection() -> ShaderReflection {
    ZoneScoped;

    ShaderReflection result = {};
    slang::ShaderReflection *program_layout = impl->slang_module->getLayout();

    auto find_entry_point_by_stage = [&](SlangStage stage) -> ls::option<u32> {
        u32 entry_point_count = program_layout->getEntryPointCount();
        for (u32 i = 0; i < entry_point_count; i++) {
            auto *entry_point = program_layout->getEntryPointByIndex(i);
            if (entry_point->getStage() == stage) {
                return i;
            }
        }

        return ls::nullopt;
    };

    // Get push constants
    u32 param_count = program_layout->getParameterCount();
    for (u32 i = 0; i < param_count; i++) {
        auto *param = program_layout->getParameterByIndex(i);
        auto *type_layout = param->getTypeLayout();
        auto *element_type_layout = type_layout->getElementTypeLayout();
        auto param_category = param->getCategory();

        if (param_category == slang::ParameterCategory::PushConstantBuffer) {
            usize push_constant_size = 0;
            auto field_count = element_type_layout->getFieldCount();
            for (u32 f = 0; f < field_count; f++) {
                auto *field_param = element_type_layout->getFieldByIndex(f);
                auto *field_type_layout = field_param->getTypeLayout();
                push_constant_size += field_type_layout->getSize();
            }

            result.pipeline_layout_id = static_cast<PipelineLayoutID>(push_constant_size / sizeof(u32));
            break;
        }
    }

    auto compute_index = find_entry_point_by_stage(SLANG_STAGE_COMPUTE);
    if (compute_index.has_value()) {
        auto *entry_point = program_layout->getEntryPointByIndex(compute_index.value());
        entry_point->getComputeThreadGroupSize(3, glm::value_ptr(result.thread_group_size));
    }

    return result;
}

auto SlangSession::destroy() -> void {
    delete impl;
    impl = nullptr;
}

auto SlangSession::load_module(const ShaderCompileInfo &info) -> ls::option<SlangModule> {
    ZoneScoped;

    slang::IModule *slang_module = {};
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    const auto &path_str = info.path.string();
    if (info.source.has_value()) {
        slang_module = impl->session->loadModuleFromSourceString("", path_str.c_str(), info.source->data(), diagnostics_blob.writeRef());
    } else {
        File file(info.path, FileAccess::Read);
        if (!file) {
            LOG_ERROR("Failed to read shader file '{}'! {}", path_str.c_str(), static_cast<usize>(file.result));
            return ls::nullopt;
        }
        auto file_data = file.whole_data();
        const char *source_data = ls::bit_cast<const char *>(file_data.get());
        slang_module = impl->session->loadModuleFromSourceString("", path_str.c_str(), source_data, diagnostics_blob.writeRef());
    }

    auto module_impl = new SlangModule::Impl;
    module_impl->slang_module = slang_module;
    module_impl->session = this;

    return SlangModule(module_impl);
}

auto SlangCompiler::create() -> ls::option<SlangCompiler> {
    ZoneScoped;

    auto impl = new Impl;
    slang::createGlobalSession(impl->global_session.writeRef());

    return SlangCompiler(impl);
}

auto SlangCompiler::destroy() -> void {
    delete impl;
    impl = nullptr;
}

auto SlangCompiler::new_session(const ShaderSessionInfo &info) -> ls::option<SlangSession> {
    ZoneScoped;

    auto slang_fs = std::make_unique<SlangVirtualFS>(info.root_directory);

    std::vector<slang::CompilerOptionEntry> entries = get_slang_entries(info.compile_flags | ShaderCompileFlag::UseScalarLayout);
    std::vector<slang::PreprocessorMacroDesc> macros;
    macros.reserve(info.definitions.size());
    for (auto &v : info.definitions) {
        macros.emplace_back(v.name.data(), v.value.data());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = impl->global_session->findProfile("spirv_1_5"),
        .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
        .forceGLSLScalarBufferLayout = true,
        .compilerOptionEntries = entries.data(),
        .compilerOptionEntryCount = static_cast<u32>(entries.size()),
    };

    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR,
        .searchPaths = nullptr,
        .searchPathCount = 0,
        .preprocessorMacros = macros.data(),
        .preprocessorMacroCount = static_cast<u32>(macros.size()),
        .fileSystem = slang_fs.get(),
    };
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(impl->global_session->createSession(session_desc, session.writeRef()))) {
        LOG_ERROR("Failed to create compiler session!");
        return ls::nullopt;
    }

    auto session_impl = new SlangSession::Impl;
    session_impl->shader_virtual_env = std::move(slang_fs);
    session_impl->session = std::move(session);

    return SlangSession(session_impl);
}

}  // namespace lr
