#include "Asset.hh"

#include "Engine/Graphics/Device.hh"
#include "Engine/OS/OS.hh"

#include "AssetParser.hh"

#include <slang-com-ptr.h>
#include <slang.h>

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
    VirtualDir &m_virtual_env;
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

    SlangVirtualFS(VirtualDir &venv, fs::path &root_dir)
        : m_virtual_env(venv),
          m_root_dir(root_dir),
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

bool AssetManager::init(this AssetManager &self, Device *device) {
    ZoneScoped;

    self.device = device;

    u32 invalid_tex_data[] = { 0xFF000000, 0xFFFF00FF, 0xFFFF00FF, 0xFF000000 };
    auto invalid_image_id = self.device->create_image(ImageInfo{
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { 2, 2, 1 },
        .debug_name = "Invalid Placeholder",
    });
    self.device->set_image_data(invalid_image_id, &invalid_tex_data, ImageLayout::ColorReadOnly);
    auto invalid_image_view_id = self.device->create_image_view(ImageViewInfo{
        .image_id = invalid_image_id,
        .type = ImageViewType::View2D,
        .debug_name = "Invalid Placeholder View",
    });
    auto invalid_sampler_id = self.device->create_cached_sampler(SamplerInfo{
        .min_filter = Filtering::Nearest,
        .mag_filter = Filtering::Nearest,
        .address_u = TextureAddressMode::Repeat,
        .address_v = TextureAddressMode::Repeat,
    });
    self.textures.try_emplace(INVALID_TEXTURE, invalid_image_id, invalid_image_view_id, invalid_sampler_id);

    return true;
}

void AssetManager::shutdown(this AssetManager &self, bool print_reports) {
    ZoneScoped;

    if (print_reports) {
        LOG_INFO("{} alive textures.", self.textures.size());
        LOG_INFO("{} alive materials.", self.materials.size());
        LOG_INFO("{} alive models.", self.models.size());
        LOG_INFO("{} alive shaders.", self.shaders.size());
    }

    for (auto &[ident, texture] : self.textures) {
        LOG_INFO("Alive texture {}", ident.sv());
        self.device->delete_images(texture.image_id);
        self.device->delete_image_views(texture.image_view_id);
        self.device->delete_samplers(texture.sampler_id);
    }

    for (auto &[ident, model] : self.models) {
        LOG_INFO("Alive model {}", ident.sv());
        if (model.vertex_buffer.has_value()) {
            auto id = model.vertex_buffer.value();
            self.device->delete_buffers(id);
        }
        if (model.index_buffer.has_value()) {
            auto id = model.index_buffer.value();
            self.device->delete_buffers(id);
        }
    }

    for (auto &[ident, shader] : self.shaders) {
        LOG_INFO("Alive shader {}", ident.sv());
        self.device->delete_shaders(shader);
    }
}

ls::option<ShaderProgram> AssetManager::load_shader_program(this AssetManager &self, const Identifier &ident, const ShaderCompileInfo &info) {
    ZoneScoped;

    /////////////////////////////////////////
    /// GLOBAL SESSION INITIALIZATION

    static Slang::ComPtr<slang::IGlobalSession> slang_global_session;
    if (!slang_global_session) {
        auto result = slang::createGlobalSession(slang_global_session.writeRef());
        if (SLANG_FAILED(result)) {
            LOG_FATAL("Cannot initialize shader compiler session! {}", result);
            return ls::nullopt;
        }
    }

    /////////////////////////////////////////
    /// SESSION INITIALIZATION

    auto root_path = fs::current_path() / "resources" / "shaders";
    SlangVirtualFS slang_fs(self.shader_virtual_env, root_path);

    std::vector<slang::CompilerOptionEntry> entries = get_slang_entries(info.compile_flags | ShaderCompileFlag::UseScalarLayout);
    std::vector<slang::PreprocessorMacroDesc> macros;
    for (ShaderPreprocessorMacroInfo &v : info.definitions) {
        macros.emplace_back(v.name.data(), v.value.data());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = slang_global_session->findProfile("spirv_1_5"),
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
        .fileSystem = &slang_fs,
    };
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
        LOG_ERROR("Failed to create compiler session!");
        return ls::nullopt;
    }

    /////////////////////////////////////////
    /////////////////////////////////////////

    // https://github.com/shader-slang/slang/blob/ed0681164d78591148781d08934676bfec63f9da/examples/cpu-com-example/main.cpp
    Slang::ComPtr<SlangCompileRequest> compile_request;
    if (SLANG_FAILED(session->createCompileRequest(compile_request.writeRef()))) {
        LOG_ERROR("Failed to create compile request!");
        return ls::nullopt;
    }

    i32 main_shader_id = compile_request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, nullptr);

    auto full_path = root_path / info.path;
    auto full_path_str = full_path.string();
    if (info.source.has_value()) {
        const char *source_data = info.source->data();
        compile_request->addTranslationUnitSourceStringSpan(main_shader_id, full_path_str.c_str(), source_data, source_data + info.source->length());
    } else {
        File file(full_path, FileAccess::Read);
        if (!file) {
            LOG_ERROR("Failed to read shader file '{}'! {}", full_path_str.c_str(), static_cast<usize>(file.result));
            return ls::nullopt;
        }
        auto file_data = file.whole_data();
        const char *source_data = ls::bit_cast<const char *>(file_data.get());
        compile_request->addTranslationUnitSourceStringSpan(main_shader_id, full_path_str.c_str(), source_data, source_data + file.size);
    }

    const SlangResult compile_result = compile_request->compile();
    const char *diagnostics = compile_request->getDiagnosticOutput();
    if (SLANG_FAILED(compile_result)) {
        LOG_ERROR("Failed to compile shader!\n{}\n", diagnostics);
        return ls::nullopt;
    }

    Slang::ComPtr<slang::IModule> shader_module;
    if (SLANG_FAILED(compile_request->getModule(main_shader_id, shader_module.writeRef()))) {
        // if this gets hit, something is def wrong
        LOG_ERROR("Failed to get shader module!");
        return ls::nullopt;
    }

    /////////////////////////////////////////
    /// REFLECTION

    ShaderProgram program = {};
    slang::ShaderReflection *program_layout = slang::ShaderReflection::get(compile_request);

    // Get Entrypoint
    u32 entry_point_count = program_layout->getEntryPointCount();
    for (u32 i = 0; i < entry_point_count; i++) {
        auto *entry_point = program_layout->getEntryPointByIndex(i);
        ShaderStageFlag entry_point_stage = ShaderStageFlag::Count;
        auto slang_stage = entry_point->getStage();

        switch (slang_stage) {
            case SLANG_STAGE_VERTEX:
                entry_point_stage = ShaderStageFlag::Vertex;
                break;
            case SLANG_STAGE_HULL:
                entry_point_stage = ShaderStageFlag::TessellationControl;
                break;
            case SLANG_STAGE_DOMAIN:
                entry_point_stage = ShaderStageFlag::TessellationEvaluation;
                break;
            case SLANG_STAGE_FRAGMENT:
                entry_point_stage = ShaderStageFlag::Fragment;
                break;
            case SLANG_STAGE_COMPUTE:
                entry_point_stage = ShaderStageFlag::Compute;
                break;
            default:
                LOG_ERROR("Unknown shader stage for {}", ident.sv());
                return ls::nullopt;
        }

        Slang::ComPtr<slang::IBlob> spirv_blob;
        if (SLANG_FAILED(compile_request->getEntryPointCodeBlob(i, 0, spirv_blob.writeRef()))) {
            LOG_ERROR("Failed to get entrypoint assembly!");
            return ls::nullopt;
        }

        ls::span<u32> spirv_data_view(ls::bit_cast<u32 *>(spirv_blob->getBufferPointer()), spirv_blob->getBufferSize() / sizeof(u32));
        auto [shader_id, shader_result] = self.device->create_shader(entry_point_stage, spirv_data_view);
        if (!shader_result) {
            return ls::nullopt;
        }

        auto &shader = program.programs.emplace_back();
        shader.shader_id = shader_id;
        shader.stage = entry_point_stage;
        if (entry_point_stage == ShaderStageFlag::Compute) {
            entry_point->getComputeThreadGroupSize(3, glm::value_ptr(shader.thread_group_size));
        }

        self.shaders.try_emplace(ident, shader_id);
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
            auto field_count = type_layout->getFieldCount();
            for (u32 f = 0; f < field_count; f++) {
                auto *field_param = element_type_layout->getFieldByIndex(f);
                auto *field_type_layout = field_param->getTypeLayout();
                push_constant_size += field_type_layout->getSize();
            }

            program.pipeline_layout_id = static_cast<PipelineLayoutID>(push_constant_size / sizeof(u32));

            break;
        }
    }

    return program;
}

Model *AssetManager::load_model(this AssetManager &self, const Identifier &ident, const fs::path &path) {
    ZoneScoped;

    auto &model = self.models[ident];
    auto model_data = AssetParser::GLTF(path);

    std::vector<Identifier> texture_idents;
    for (auto &v : model_data->textures) {
        std::unique_ptr<ImageAssetData> image_data;
        switch (v.file_type) {
            case AssetFileType::PNG:
            case AssetFileType::JPEG:
                image_data = AssetParser::STB(v.data.data(), v.data.size());
                break;
            default:;
        }

        if (image_data) {
            auto new_ident = Identifier::random();
            Extent3D extent = { image_data->width, image_data->height, 1 };

            auto &texture = self.textures[new_ident];
            texture.image_id = self.device->create_image(ImageInfo{
                .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
                .format = image_data->format,
                .type = ImageType::View2D,
                .extent = extent,
            });
            self.device->set_image_data(texture.image_id, image_data->data, ImageLayout::ColorReadOnly);
            texture.image_view_id = self.device->create_image_view(ImageViewInfo{
                .image_id = texture.image_id,
                .type = ImageViewType::View2D,
            });

            if (auto sampler_index = v.sampler_index; sampler_index.has_value()) {
                auto &sampler = model_data->samplers[sampler_index.value()];

                SamplerInfo sampler_info = {
                    .min_filter = sampler.min_filter,
                    .mag_filter = sampler.mag_filter,
                    .address_u = sampler.address_u,
                    .address_v = sampler.address_v,
                };
                texture.sampler_id = self.device->create_cached_sampler(sampler_info);
            }

            texture_idents.push_back(new_ident);
        } else {
            LOG_ERROR("An image named {} could not be parsed!", v.name);
            texture_idents.push_back(INVALID_TEXTURE);
        }
    }

    std::vector<Identifier> material_idents;
    for (auto &v : model_data->materials) {
        auto new_ident = Identifier::random();

        Material material = {};
        material.albedo_color = v.albedo_color;
        material.emissive_color = v.emissive_color;
        material.roughness_factor = v.roughness_factor;
        material.metallic_factor = v.metallic_factor;
        material.alpha_mode = v.alpha_mode;
        material.alpha_cutoff = v.alpha_cutoff;

        if (auto i = v.albedo_image_data_index; i.has_value()) {
            material.albedo_texture = texture_idents[i.value()];
        }
        if (auto i = v.normal_image_data_index; i.has_value()) {
            material.normal_texture = texture_idents[i.value()];
        }
        if (auto i = v.emissive_image_data_index; i.has_value()) {
            material.emissive_texture = texture_idents[i.value()];
        }

        self.add_material(new_ident, material);
        material_idents.push_back(new_ident);
    }

    for (auto &v : model_data->primitives) {
        auto &primitive = model.primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset.value();
        primitive.vertex_count = v.vertex_count.value();
        primitive.index_offset = v.index_offset.value();
        primitive.index_count = v.index_count.value();

        if (auto i = v.material_index; i.has_value()) {
            primitive.material_ident = material_idents[i.value()];
        }
    }

    for (auto &v : model_data->meshes) {
        auto &mesh = model.meshes.emplace_back();
        mesh.name = v.name;
        mesh.primitive_indices = v.primitive_indices;
    }

    self.device->wait_for_work();
    auto &queue = self.device->queue_at(CommandType::Transfer);
    auto &staging_buffer = self.device->staging_buffer_at(0);
    staging_buffer.reset();

    {
        usize vertex_upload_size = model_data->vertices.size() * sizeof(Vertex);
        auto vertex_alloc = staging_buffer.alloc(vertex_upload_size);
        std::memcpy(vertex_alloc.ptr, model_data->vertices.data(), vertex_upload_size);

        model.vertex_buffer = self.device->create_buffer(BufferInfo{
            .usage_flags = BufferUsage::TransferDst | BufferUsage::Vertex,
            .flags = MemoryFlag::Dedicated,
            .preference = MemoryPreference::Device,
            .data_size = vertex_upload_size,
            .debug_name = "Model Vertex Buffer",
        });

        auto cmd_list = queue.begin_command_list(0);
        BufferCopyRegion copy_region = {
            .src_offset = vertex_alloc.offset,
            .dst_offset = 0,
            .size = vertex_upload_size,
        };
        cmd_list.copy_buffer_to_buffer(vertex_alloc.buffer_id, model.vertex_buffer.value(), copy_region);
        queue.end_command_list(cmd_list);
        queue.submit(0, { .self_wait = true });
        queue.wait_for_work();
        staging_buffer.reset();
    }

    {
        usize index_upload_size = model_data->indices.size() * sizeof(u32);
        auto index_alloc = staging_buffer.alloc(index_upload_size);
        std::memcpy(index_alloc.ptr, model_data->indices.data(), index_upload_size);

        model.index_buffer = self.device->create_buffer(BufferInfo{
            .usage_flags = BufferUsage::TransferDst | BufferUsage::Index,
            .flags = MemoryFlag::Dedicated,
            .preference = MemoryPreference::Device,
            .data_size = index_upload_size,
            .debug_name = "Model Index Buffer",
        });

        auto cmd_list = queue.begin_command_list(0);
        BufferCopyRegion copy_region = {
            .src_offset = index_alloc.offset,
            .dst_offset = 0,
            .size = index_upload_size,
        };
        cmd_list.copy_buffer_to_buffer(index_alloc.buffer_id, model.index_buffer.value(), copy_region);
        queue.end_command_list(cmd_list);
        queue.submit(0, {});
        queue.wait_for_work();
        staging_buffer.reset();
    }

    return &model;
}

Material *AssetManager::add_material(this AssetManager &self, const Identifier &ident, const Material &material_data) {
    ZoneScoped;

    auto material_iter = self.materials.try_emplace(ident, material_data);
    auto &material = material_iter.first->second;

    return &material;
}

ls::option<ShaderID> AssetManager::shader_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.shaders.find(ident);
    if (it == self.shaders.end()) {
        return ls::nullopt;
    }

    return it->second;
}

Texture *AssetManager::texture_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.textures.find(ident);
    if (it == self.textures.end()) {
        return nullptr;
    }

    return &it->second;
}

Material *AssetManager::material_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.materials.find(ident);
    if (it == self.materials.end()) {
        return nullptr;
    }

    return &it->second;
}

Model *AssetManager::model_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.models.find(ident);
    if (it == self.models.end()) {
        return nullptr;
    }

    return &it->second;
}

}  // namespace lr
