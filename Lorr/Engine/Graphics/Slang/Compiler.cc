#include "Engine/Graphics/Slang/Compiler.hh"

#include "Engine/Memory/Stack.hh"

#include "Engine/OS/File.hh"

#include <slang-com-ptr.h>
#include <slang.h>

namespace lr {
struct SlangBlobSpan : ISlangBlob {
    ls::span<u8> m_data = {};
    std::atomic_uint32_t m_refCount = 1;

    ISlangUnknown *getInterface(SlangUUID const &) {
        return nullptr;
    }
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid, void **outObject) SLANG_OVERRIDE {
        ISlangUnknown *intf = getInterface(uuid);
        if (intf) {
            addRef();
            *outObject = intf;
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }

    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override {
        return ++m_refCount;
    }

    SLANG_NO_THROW uint32_t SLANG_MCALL release() override {
        --m_refCount;
        if (m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }

    SlangBlobSpan(ls::span<u8> data) : m_data(data) {}
    virtual ~SlangBlobSpan() = default;
    SLANG_NO_THROW void const *SLANG_MCALL getBufferPointer() final {
        return m_data.data();
    };
    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() final {
        return m_data.size();
    };
};

// PERF: When we are at Editor environment, shaders obviously needs to be loaded
// through file system. But when we are at runtime environment, we don't need
// file system because we probably would have proper asset manager with all
// shaders are preloaded into virtual environment, so ::loadFile would just
// return already existing shader file.
struct SlangVirtualFS : ISlangFileSystem {
    ankerl::unordered_dense::map<std::string, std::vector<u8>> m_loaded_modules;
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

    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override {
        return ++m_refCount;
    }

    SLANG_NO_THROW uint32_t SLANG_MCALL release() override {
        --m_refCount;
        if (m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }

    SlangVirtualFS(fs::path root_dir) : m_loaded_modules({}), m_root_dir(std::move(root_dir)), m_refCount(1) {};
    virtual ~SlangVirtualFS() = default;

    ISlangUnknown *getInterface(SlangUUID const &) {
        return nullptr;
    }
    SLANG_NO_THROW void *SLANG_MCALL castAs(const SlangUUID &) final {
        return nullptr;
    }

    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const *path_cstr, ISlangBlob **outBlob) final {
        auto path = fs::path(path_cstr);

        // /resources/shaders/path/to/xx.slang -> path.to.xx
        auto module_name = fs::relative(path, m_root_dir).replace_extension("").string();
        std::ranges::replace(module_name, static_cast<c8>(fs::path::preferred_separator), '.');

        auto it = m_loaded_modules.find(module_name);
        if (it == m_loaded_modules.end()) {
            auto file_bytes = File::to_bytes(path);
            if (!file_bytes.empty()) {
                auto new_it = m_loaded_modules.emplace(module_name, std::move(file_bytes));
                *outBlob = new SlangBlobSpan(new_it.first->second);
                return SLANG_OK;
            } else {
                return SLANG_E_NOT_FOUND;
            }
        } else {
            *outBlob = new SlangBlobSpan(it->second);
            return SLANG_OK;
        }
    }
};

template<>
struct Handle<SlangModule>::Impl {
    SlangSession session = {};
    slang::IModule *slang_module = nullptr;
};

template<>
struct Handle<SlangSession>::Impl {
    std::unique_ptr<SlangVirtualFS> virtual_fs;
    Slang::ComPtr<slang::ISession> session;
};

template<>
struct Handle<SlangCompiler>::Impl {
    Slang::ComPtr<slang::IGlobalSession> global_session;
};

auto SlangModule::destroy() -> void {
    delete impl;
    impl = nullptr;
}

auto SlangModule::get_entry_point(std::string_view name) -> ls::option<SlangEntryPoint> {
    ZoneScoped;
    memory::ScopedStack stack;

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    if (SLANG_FAILED(impl->slang_module->findEntryPointByName(stack.null_terminate_cstr(name), entry_point.writeRef()))) {
        LOG_ERROR("Shader entry point '{}' is not found.", name);
        return {};
    }

    std::vector<slang::IComponentType *> component_types;
    component_types.push_back(impl->slang_module);
    component_types.push_back(entry_point);

    Slang::ComPtr<slang::IComponentType> composed_program;
    {
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        auto result = impl->session->session->createCompositeComponentType(
            component_types.data(),
            u32(component_types.size()),
            composed_program.writeRef(),
            diagnostics_blob.writeRef()
        );
        if (diagnostics_blob) {
            LOG_WARN("{}", (const char *)diagnostics_blob->getBufferPointer());
        }

        if (SLANG_FAILED(result)) {
            LOG_ERROR("Failed to composite shader module.");
            return ls::nullopt;
        }
    }

    Slang::ComPtr<slang::IComponentType> linked_program;
    {
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
        if (diagnostics_blob) {
            LOG_WARN("{}", (const char *)diagnostics_blob->getBufferPointer());
        }
    }

    Slang::ComPtr<slang::IBlob> spirv_code;
    {
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        auto result = linked_program->getEntryPointCode(0, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());
        if (diagnostics_blob) {
            LOG_WARN("{}", (const char *)diagnostics_blob->getBufferPointer());
        }

        if (SLANG_FAILED(result)) {
            LOG_ERROR("Failed to compile shader module.\n{}", (const char *)diagnostics_blob->getBufferPointer());
            return ls::nullopt;
        }
    }

    auto ir = std::vector<u32>(spirv_code->getBufferSize() / 4);
    std::memcpy(ir.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

    return SlangEntryPoint{
        .ir = std::move(ir),
    };
}

auto SlangModule::get_reflection() -> ShaderReflection {
    ZoneScoped;

    ShaderReflection result = {};
    slang::ShaderReflection *program_layout = impl->slang_module->getLayout();
    ls::option<u32> compute_entry_point_index = ls::nullopt;

    u32 entry_point_count = program_layout->getEntryPointCount();
    for (u32 i = 0; i < entry_point_count; i++) {
        auto *entry_point = program_layout->getEntryPointByIndex(i);
        if (entry_point->getStage() == SLANG_STAGE_COMPUTE) {
            compute_entry_point_index = i;
            break;
        }
    }

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

            result.pipeline_layout_index = (push_constant_size / sizeof(u32));
            break;
        }
    }

    if (compute_entry_point_index.has_value()) {
        auto *entry_point = program_layout->getEntryPointByIndex(compute_entry_point_index.value());
        entry_point->getComputeThreadGroupSize(3, glm::value_ptr(result.thread_group_size));
    }

    return result;
}

auto SlangModule::session() -> SlangSession {
    return impl->session;
}

auto SlangSession::destroy() -> void {
    delete impl;
    impl = nullptr;
}

auto SlangSession::load_module(const SlangModuleInfo &info) -> ls::option<SlangModule> {
    ZoneScoped;
    memory::ScopedStack stack;

    slang::IModule *slang_module = {};
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    auto source = std::string{};

    if (info.source.has_value()) {
        source = info.source.value();
    } else {
        auto path = this->modular_path(info.module_name);
        source = File::to_string(path);
        if (source.empty()) {
            LOG_ERROR("Failed to read shader file '{}'!", path);
            return ls::nullopt;
        }
    }

    slang_module =
        impl->session->loadModuleFromSourceString(info.module_name.c_str(), info.module_name.c_str(), source.c_str(), diagnostics_blob.writeRef());

    if (diagnostics_blob) {
        LOG_WARN("{}", (const char *)diagnostics_blob->getBufferPointer());
    }

    auto module_impl = new SlangModule::Impl;
    module_impl->slang_module = slang_module;
    module_impl->session = SlangSession(impl);

    return SlangModule(module_impl);
}

auto SlangSession::root_directory() const -> const fs::path & {
    ZoneScoped;

    return impl->virtual_fs->m_root_dir;
}

auto SlangSession::modular_path(const std::string &module_name) -> fs::path {
    ZoneScoped;

    // path.to.xx -> /resources/shaders/path/to/xx.slang
    std::string module_name_path = module_name;
    std::ranges::replace(module_name_path, '.', static_cast<c8>(fs::path::preferred_separator));
    module_name_path += ".slang";

    return this->root_directory() / module_name_path;
}

auto SlangCompiler::create() -> ls::option<SlangCompiler> {
    ZoneScoped;

    auto impl = new Impl;
    slang::createGlobalSession(impl->global_session.writeRef());
    return SlangCompiler(impl);
}

auto SlangCompiler::destroy() -> void {
    ZoneScoped;

    delete impl;
    impl = nullptr;
}

auto SlangCompiler::new_session(const SlangSessionInfo &info) -> ls::option<SlangSession> {
    ZoneScoped;

    auto slang_fs = std::make_unique<SlangVirtualFS>(info.root_directory);

    // clang-format off
    slang::CompilerOptionEntry entries[] = {
        { .name = slang::CompilerOptionName::Optimization, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL } },
#if LS_DEBUG
        { .name = slang::CompilerOptionName::DebugInformationFormat, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = SLANG_DEBUG_INFO_FORMAT_DEFAULT } },
        { .name = slang::CompilerOptionName::DebugInformation, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL } },
#endif
        { .name = slang::CompilerOptionName::UseUpToDateBinaryModule, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1 } },
        { .name = slang::CompilerOptionName::GLSLForceScalarLayout, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1 } },
        { .name = slang::CompilerOptionName::Language, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "slang" } },
        { .name = slang::CompilerOptionName::VulkanUseEntryPointName, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1 } },
        { .name = slang::CompilerOptionName::DisableWarning, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "39001" } },
        { .name = slang::CompilerOptionName::DisableWarning, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "41012" } },
        { .name = slang::CompilerOptionName::DisableWarning, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "41017" } },
        { .name = slang::CompilerOptionName::Capability, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "vk_mem_model" } },
        { .name = slang::CompilerOptionName::Capability, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "spvGroupNonUniformBallot" } },
        { .name = slang::CompilerOptionName::Capability, .value = { .kind = slang::CompilerOptionValueKind::String, .stringValue0 = "spvGroupNonUniformShuffle" } },
        // { .name = slang::CompilerOptionName::DumpIntermediates, .value = { .kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1 } },
    };
    // clang-format on

    auto macros = std::vector<slang::PreprocessorMacroDesc>();
    for (const auto &[macro_name, macro_value] : info.definitions) {
        macros.emplace_back(macro_name.c_str(), macro_value.c_str());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = impl->global_session->findProfile("spirv_1_5"),
        .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
        .floatingPointMode = SLANG_FLOATING_POINT_MODE_FAST,
        .lineDirectiveMode = SLANG_LINE_DIRECTIVE_MODE_STANDARD,
        .forceGLSLScalarBufferLayout = true,
        .compilerOptionEntries = entries,
        .compilerOptionEntryCount = static_cast<u32>(ls::count_of(entries)),
    };

    const auto search_path = info.root_directory.string();
    const auto *search_path_cstr = search_path.c_str();
    const c8 *search_paths[] = { search_path_cstr };
    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .searchPaths = search_paths,
        .searchPathCount = ls::count_of(search_paths),
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
    session_impl->virtual_fs = std::move(slang_fs);
    session_impl->session = std::move(session);

    return SlangSession(session_impl);
}

} // namespace lr
