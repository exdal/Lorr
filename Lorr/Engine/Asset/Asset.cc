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
    SLANG_IUNKNOWN_QUERY_INTERFACE
    SLANG_IUNKNOWN_ADD_REF
    SLANG_IUNKNOWN_RELEASE

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

    SLANG_IUNKNOWN_QUERY_INTERFACE
    SLANG_IUNKNOWN_ADD_REF
    SLANG_IUNKNOWN_RELEASE

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

        // /resources/shaders/xx.slang -> shader://xx
        auto module_name = fs::relative(path, m_root_dir).replace_extension("").string();
        std::replace(module_name.begin(), module_name.end(), fs::path::preferred_separator, '.');

        auto it = m_virtual_env.files.find(module_name);
        if (it == m_virtual_env.files.end()) {
            if (auto result = m_virtual_env.read_file(module_name, path); result.has_value()) {
                auto &new_file = result.value().get();
                *outBlob = new SlangBlob(std::vector<u8>{ new_file.data(), (new_file.data() + new_file.size()) });

                LR_LOG_TRACE("New shader module '{}' is loaded.", module_name);
                return SLANG_OK;
            } else {
                LR_LOG_ERROR("Failed to load shader '{}'!", path.c_str());
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
    self.material_buffer_id = self.device->create_buffer(BufferInfo{
        .usage_flags = BufferUsage::TransferDst,
        .flags = MemoryFlag::Dedicated,
        .preference = MemoryPreference::Device,
        .data_size = sizeof(GPUMaterial) * MAX_MATERIAL_COUNT,
        .debug_name = "Material Buffer",
    });

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
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .type = ImageViewType::View2D,
        .debug_name = "Invalid Placeholder View",
    });
    auto invalid_sampler_id = self.device->create_cached_sampler(SamplerInfo{
        .min_filter = Filtering::Nearest,
        .mag_filter = Filtering::Nearest,
        .address_u = TextureAddressMode::Repeat,
        .address_v = TextureAddressMode::Repeat,
        .max_lod = 10000.0f,
    });
    self.textures.emplace_back(invalid_image_id, invalid_image_view_id, invalid_sampler_id);

    /// ENGINE RESOURCES ///
    self.load_shader("shader://imgui_vs", { .entry_point = "vs_main", .path = "imgui.slang" });
    self.load_shader("shader://imgui_fs", { .entry_point = "fs_main", .path = "imgui.slang" });
    self.load_shader("shader://fullscreen_vs", { .entry_point = "vs_main", .path = "fullscreen.slang" });
    self.load_shader("shader://fullscreen_fs", { .entry_point = "fs_main", .path = "fullscreen.slang" });
    self.load_shader("shader://atmos.transmittance", { .entry_point = "cs_main", .path = "atmos/transmittance.slang" });
    self.load_shader("shader://editor.grid_vs", { .entry_point = "vs_main", .path = "editor/grid.slang" });
    self.load_shader("shader://editor.grid_fs", { .entry_point = "fs_main", .path = "editor/grid.slang" });

    return true;
}

ls::option<ModelID> AssetManager::load_model(this AssetManager &self, const fs::path &path) {
    ZoneScoped;

    File model_file(path, FileAccess::Read);
    if (!model_file) {
        return ls::nullopt;
    }

    usize model_id = self.models.size();
    auto &model = self.models.emplace_back();
    auto model_file_data = model_file.whole_data();
    auto model_data = parse_model_gltf(model_file_data.get(), model_file.size);

    usize texture_offset = self.textures.size();
    for (auto &v : model_data->textures) {
        auto &texture = self.textures.emplace_back();
        std::unique_ptr<ImageAssetData> image_data;
        switch (v.file_type) {
            case AssetFileType::PNG:
            case AssetFileType::JPEG:
                image_data = parse_image_stbi(v.data.data(), v.data.size());
                break;
            default:
                break;
        }

        if (image_data) {
            Extent3D extent = { image_data->width, image_data->height, 1 };

            texture.image_id = self.device->create_image(ImageInfo{
                .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
                .format = image_data->format,
                .type = ImageType::View2D,
                .extent = extent,
            });
            self.device->set_image_data(texture.image_id, image_data->data, ImageLayout::ColorReadOnly);
            texture.image_view_id = self.device->create_image_view(ImageViewInfo{
                .image_id = texture.image_id,
                .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
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
        } else {
            LR_LOG_ERROR("An image named {} could not be parsed!", v.name);
            texture = self.textures[0];
        }
    }

    usize material_offset = self.materials.size();
    for (auto &v : model_data->materials) {
        Material material = {};
        material.albedo_color = v.albedo_color;
        material.emissive_color = v.emissive_color;
        material.roughness_factor = v.roughness_factor;
        material.metallic_factor = v.metallic_factor;
        material.alpha_mode = v.alpha_mode;
        material.alpha_cutoff = v.alpha_cutoff;

        if (auto i = v.albedo_image_data_index; i.has_value()) {
            material.albedo_texture_index = i.value() + texture_offset;
        }
        if (auto i = v.normal_image_data_index; i.has_value()) {
            material.normal_texture_index = i.value() + texture_offset;
        }
        if (auto i = v.emissive_image_data_index; i.has_value()) {
            material.emissive_texture_index = i.value() + texture_offset;
        }

        self.add_material(material);
    }

    for (auto &v : model_data->primitives) {
        auto &primitive = model.primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset.value();
        primitive.vertex_count = v.vertex_count.value();
        primitive.index_offset = v.index_offset.value();
        primitive.index_count = v.index_count.value();

        if (auto material_index = v.material_index; material_index.has_value()) {
            primitive.material_index = material_offset + material_index.value();
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

    return static_cast<ModelID>(model_id);
}

ls::option<MaterialID> AssetManager::add_material(this AssetManager &self, const Material &material) {
    ZoneScoped;

    auto &transfer_queue = self.device->queue_at(CommandType::Transfer);
    auto cmd_list = transfer_queue.begin_command_list(0);

    BufferID temp_buffer = self.device->create_buffer(BufferInfo{
        .usage_flags = BufferUsage::TransferSrc,
        .flags = MemoryFlag::HostSeqWrite,
        .preference = MemoryPreference::Host,
        .data_size = sizeof(GPUMaterial),
    });
    transfer_queue.defer(temp_buffer);

    auto gpu_material = self.device->buffer_host_data<GPUMaterial>(temp_buffer);
    gpu_material->albedo_color = material.albedo_color;
    gpu_material->emissive_color = material.emissive_color;
    gpu_material->roughness_factor = material.roughness_factor;
    gpu_material->metallic_factor = material.metallic_factor;
    gpu_material->alpha_mode = material.alpha_mode;
    gpu_material->alpha_cutoff = material.alpha_cutoff;
    {
        auto &texture = self.textures[material.albedo_texture_index.value_or(0)];
        gpu_material->albedo_image_view = texture.image_view_id;
        gpu_material->albedo_sampler = texture.sampler_id;
    }
    {
        auto &texture = self.textures[material.normal_texture_index.value_or(0)];
        gpu_material->normal_image_view = texture.image_view_id;
        gpu_material->normal_sampler = texture.sampler_id;
    }
    {
        auto &texture = self.textures[material.emissive_texture_index.value_or(0)];
        gpu_material->emissive_image_view = texture.image_view_id;
        gpu_material->emissive_sampler = texture.sampler_id;
    }

    BufferCopyRegion copy_region = {
        .src_offset = 0,
        .dst_offset = self.materials.size() * sizeof(GPUMaterial),
        .size = sizeof(GPUMaterial),
    };
    cmd_list.copy_buffer_to_buffer(temp_buffer, self.material_buffer_id, copy_region);
    transfer_queue.end_command_list(cmd_list);
    transfer_queue.submit(0, { .self_wait = false });

    usize material_index = self.materials.size();
    self.materials.push_back(material);

    return static_cast<MaterialID>(material_index);
}

ls::option<ShaderID> AssetManager::load_shader(this AssetManager &self, Identifier ident, const ShaderCompileInfo &info) {
    ZoneScoped;

    /////////////////////////////////////////
    /// GLOBAL SESSION INITIALIZATION

    static Slang::ComPtr<slang::IGlobalSession> slang_global_session;
    if (!slang_global_session) {
        auto result = slang::createGlobalSession(slang_global_session.writeRef());
        if (SLANG_FAILED(result)) {
            LR_LOG_FATAL("Cannot initialize shader compiler session! {}", result);
            return ls::nullopt;
        }
    }

    /////////////////////////////////////////
    /// SESSION INITIALIZATION

    std::vector<slang::CompilerOptionEntry> entries = get_slang_entries(info.compile_flags);
    std::vector<slang::PreprocessorMacroDesc> macros;
    for (ShaderPreprocessorMacroInfo &v : info.definitions) {
        macros.emplace_back(v.name.data(), v.value.data());
    }

    slang::TargetDesc target_desc = {
        .format = SLANG_SPIRV,
        .profile = slang_global_session->findProfile("spirv_1_4"),
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
    };
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
        LR_LOG_ERROR("Failed to create compiler session!");
        return ls::nullopt;
    }

    /////////////////////////////////////////
    /////////////////////////////////////////

    // https://github.com/shader-slang/slang/blob/ed0681164d78591148781d08934676bfec63f9da/examples/cpu-com-example/main.cpp
    Slang::ComPtr<SlangCompileRequest> compile_request;
    if (SLANG_FAILED(session->createCompileRequest(compile_request.writeRef()))) {
        LR_LOG_ERROR("Failed to create compile request!");
        return ls::nullopt;
    }

    auto root_path = fs::current_path() / "resources" / "shaders";
    SlangVirtualFS slang_fs(self.shader_virtual_env, root_path);
    compile_request->setFileSystem(&slang_fs);

    i32 main_shader_id = compile_request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, nullptr);

    auto full_path = root_path / info.path;
    if (info.source.has_value()) {
        const char *source_data = info.source->data();
        compile_request->addTranslationUnitSourceStringSpan(main_shader_id, full_path.c_str(), source_data, source_data + info.source->length());
    } else {
        File file(full_path, FileAccess::Read);
        if (!file) {
            LR_LOG_ERROR("Failed to read shader file '{}'! {}", full_path.c_str(), static_cast<usize>(file.result));
            return ls::nullopt;
        }
        auto file_data = file.whole_data();
        const char *source_data = ls::bit_cast<const char *>(file_data.get());
        compile_request->addTranslationUnitSourceStringSpan(main_shader_id, full_path.c_str(), source_data, source_data + file.size);
    }

    const SlangResult compile_result = compile_request->compile();
    const char *diagnostics = compile_request->getDiagnosticOutput();
    if (SLANG_FAILED(compile_result)) {
        LR_LOG_ERROR("Failed to compile shader!\n{}\n", diagnostics);
        return ls::nullopt;
    }

    Slang::ComPtr<slang::IModule> shader_module;
    if (SLANG_FAILED(compile_request->getModule(main_shader_id, shader_module.writeRef()))) {
        // if this gets hit, something is def wrong
        LR_LOG_ERROR("Failed to get shader module!");
        return ls::nullopt;
    }

    /////////////////////////////////////////
    /// REFLECTION

    SlangReflection *reflection = compile_request->getReflection();
    u32 entry_point_count = spReflection_getEntryPointCount(reflection);
    u32 found_entry_point_id = ~0_u32;
    ShaderStageFlag found_shader_stage = ShaderStageFlag::Count;
    for (u32 i = 0; i < entry_point_count; i++) {
        SlangReflectionEntryPoint *entry_point = spReflection_getEntryPointByIndex(reflection, i);
        std::string_view entry_point_name = spReflectionEntryPoint_getName(entry_point);
        if (entry_point_name == info.entry_point) {
            found_entry_point_id = i;
            auto slang_stage = spReflectionEntryPoint_getStage(entry_point);
            switch (slang_stage) {
                case SLANG_STAGE_VERTEX:
                    found_shader_stage = ShaderStageFlag::Vertex;
                    break;
                case SLANG_STAGE_HULL:
                    found_shader_stage = ShaderStageFlag::TessellationControl;
                    break;
                case SLANG_STAGE_DOMAIN:
                    found_shader_stage = ShaderStageFlag::TessellationEvaluation;
                    break;
                case SLANG_STAGE_FRAGMENT:
                    found_shader_stage = ShaderStageFlag::Fragment;
                    break;
                case SLANG_STAGE_COMPUTE:
                    found_shader_stage = ShaderStageFlag::Compute;
                    break;
                    break;
                default:
                    break;
            }
            break;
        }
    }

    if (found_entry_point_id == ~0u) {
        LR_LOG_ERROR("Failed to find given entry point ''!", info.entry_point);
        return ls::nullopt;
    }

    /////////////////////////////////////////
    /// RESULT BLOB/CLEAN UP

    Slang::ComPtr<slang::IBlob> spirv_blob;
    if (SLANG_FAILED(compile_request->getEntryPointCodeBlob(found_entry_point_id, 0, spirv_blob.writeRef()))) {
        LR_LOG_ERROR("Failed to get entrypoint assembly!");
        return ls::nullopt;
    }

    ls::span<u32> spirv_data_view(ls::bit_cast<u32 *>(spirv_blob->getBufferPointer()), spirv_blob->getBufferSize() / sizeof(u32));
    auto [shader_id, shader_result] = self.device->create_shader(found_shader_stage, spirv_data_view);
    if (!shader_result) {
        return ls::nullopt;
    }

    self.shaders.emplace(ident, shader_id);

    return shader_id;
}

ls::option<ShaderID> AssetManager::shader_at(this AssetManager &self, Identifier ident) {
    ZoneScoped;

    auto it = self.shaders.find(ident);
    if (it == self.shaders.end()) {
        return ls::nullopt;
    }

    return it->second;
}

}  // namespace lr
