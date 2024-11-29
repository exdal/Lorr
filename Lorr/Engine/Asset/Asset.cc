#include "Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"
#include "Engine/Memory/Hasher.hh"
#include "Engine/Memory/PagedPool.hh"
#include "Engine/OS/OS.hh"

namespace lr {
template<>
struct Handle<AssetManager>::Impl {
    Device device = {};
    fs::path root_path = fs::current_path();

    Buffer material_buffer = {};
    ankerl::unordered_dense::map<Identifier, Asset> registry = {};
    PagedPool<Model, ModelID> models = {};
    PagedPool<Texture, TextureID> textures = {};
    PagedPool<Material, MaterialID, 1024_sz> materials = {};
};

auto AssetManager::create(Device_H device) -> AssetManager {
    ZoneScoped;

    auto impl = new Impl;
    auto self = AssetManager(impl);

    impl->device = device;
    impl->material_buffer = Buffer::create(
                                device,
                                vk::BufferUsage::Storage | vk::BufferUsage::TransferDst,
                                impl->materials.max_resources() * sizeof(GPUMaterial),
                                vk::MemoryAllocationUsage::PreferDevice)
                                .value()
                                .set_name("Material Buffer");

    self.set_root_path(fs::current_path());

    return self;
}

auto AssetManager::destroy() -> void {
    ZoneScoped;
}

auto AssetManager::set_root_path(const fs::path &path) -> void {
    impl->root_path = path;
}

auto AssetManager::asset_root_path(AssetType type) -> fs::path {
    ZoneScoped;

    auto root = impl->root_path / "resources";
    switch (type) {
        case AssetType::Root:
            return root;
        case AssetType::Shader:
            return root / "shaders";
        case AssetType::Model:
            return root / "models";
        case AssetType::Texture:
            return root / "textures";
        case AssetType::Material:
            return root / "materials";
        case AssetType::Font:
            return root / "fonts";
        case AssetType::Scene:
            return root / "scenes";
    }
}

auto AssetManager::to_asset_file_type(const fs::path &path) -> AssetFileType {
    ZoneScoped;
    memory::ScopedStack stack;

    if (!path.has_extension()) {
        return AssetFileType::None;
    }

    auto extension = stack.to_upper(path.extension().string());
    switch (fnv64_str(extension)) {
        case fnv64_c(".GLB"):
            return AssetFileType::GLB;
        case fnv64_c(".PNG"):
            return AssetFileType::PNG;
        case fnv64_c(".JPG"):
        case fnv64_c(".JPEG"):
            return AssetFileType::JPEG;
        default:
            return AssetFileType::None;
    }
}

auto AssetManager::material_buffer() const -> Buffer & {
    return impl->material_buffer;
}

auto AssetManager::register_asset(const Identifier &ident, const fs::path &path, AssetType type) -> Asset * {
    ZoneScoped;

    Asset asset = {};
    asset.path = path;
    asset.type = type;

    switch (type) {
        case AssetType::Model: {
            auto model_result = impl->models.create();
            if (!model_result.has_value()) {
                LOG_ERROR("Failed to create new model, out of pool space.");
                return nullptr;
            }

            asset.model_id = model_result->id;
            break;
        }
        case AssetType::Texture: {
            auto texture_result = impl->textures.create();
            if (!texture_result.has_value()) {
                LOG_ERROR("Failed to create new texture, out of pool space.");
                return nullptr;
            }

            asset.texture_id = texture_result->id;
            break;
        }
        case AssetType::Material: {
            auto material_result = impl->materials.create();
            if (!material_result.has_value()) {
                LOG_ERROR("Failed to create new material, out of pool space.");
                return nullptr;
            }

            asset.material_id = material_result->id;
            break;
        }
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
        case AssetType::Scene:
            LS_EXPECT(false);  // TODO: Other asset types
            return nullptr;
    }

    auto [asset_it, inserted] = impl->registry.try_emplace(ident, asset);
    if (!inserted) {
        return nullptr;
    }

    return &asset_it->second;
}

auto AssetManager::load_texture(const Identifier &ident, vk::Format format, vk::Extent3D extent, ls::span<u8> pixels, Sampler sampler)
    -> TextureID {
    ZoneScoped;

    auto image = Image::create(
        impl->device,
        vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
        format,
        vk::ImageType::View2D,
        extent,
        vk::ImageAspectFlag::Color,
        1,
        static_cast<u32>(glm::floor(glm::log2(static_cast<f32>(ls::max(extent.width, extent.height)))) + 1));
    if (!image.has_value()) {
        return TextureID::Invalid;
    }

    image->set_name(std::string(ident.sv()))
        .set_data(pixels.data(), pixels.size(), vk::ImageLayout::TransferDst)
        .generate_mips(vk::ImageLayout::TransferDst)
        .transition(vk::ImageLayout::TransferDst, vk::ImageLayout::ShaderReadOnly);

    auto *asset = this->register_asset(ident, "", AssetType::Texture);
    if (!asset) {
        return TextureID::Invalid;
    }

    auto *texture = this->get_texture(asset->texture_id);
    texture->image = image.value();
    if (sampler) {
        texture->sampler = sampler;
    } else {
        texture->sampler = Sampler::create(
                               impl->device,
                               vk::Filtering::Linear,
                               vk::Filtering::Linear,
                               vk::Filtering::Linear,
                               vk::SamplerAddressMode::Repeat,
                               vk::SamplerAddressMode::Repeat,
                               vk::SamplerAddressMode::Repeat,
                               vk::CompareOp::Never)
                               .value()
                               .set_name("Linear Repeat Sampler");
    }

    return asset->texture_id;
}

auto AssetManager::load_texture(const Identifier &ident, const fs::path &path, Sampler sampler) -> TextureID {
    ZoneScoped;

    if (!path.has_extension()) {
        LOG_ERROR("Trying to load texture \"{}\" file without an extension.", ident.sv());
        return TextureID::Invalid;
    }

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Cannot read texture file.");
        return TextureID::Invalid;
    }

    auto file_data = file.whole_data();
    vk::Format format = {};
    vk::Extent3D extent = {};
    std::vector<u8> pixels = {};

    switch (this->to_asset_file_type(path)) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            auto image = STBImageInfo::parse(ls::span{ file_data.get(), file.size });
            if (!image.has_value()) {
                return TextureID::Invalid;
            }
            format = image->format;
            extent = image->extent;
            pixels = std::move(image->data);
            break;
        }
        case AssetFileType::None:
        case AssetFileType::GLB:
        default:
            return TextureID::Invalid;
    }

    return this->load_texture(ident, format, extent, pixels, sampler);
}

auto AssetManager::load_model(const Identifier &ident, const fs::path &path) -> ModelID {
    ZoneScoped;

    auto model_info = GLTFModelInfo::parse(path);
    if (!model_info.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", path);
        return ModelID::Invalid;
    }

    std::vector<TextureID> textures = {};
    textures.reserve(model_info->textures.size());
    for (auto &v : model_info->textures) {
        if (!v.image_index.has_value()) {
            LOG_ERROR("Model {} has invalid image.", ident.sv());
            return ModelID::Invalid;
        }

        auto &image_info = model_info->images[v.image_index.value()];
        Sampler sampler = {};
        if (v.sampler_index.has_value()) {
            auto &sampler_info = model_info->samplers[v.sampler_index.value()];
            sampler = Sampler::create(
                          impl->device,
                          sampler_info.min_filter,
                          sampler_info.mag_filter,
                          vk::Filtering::Linear,
                          sampler_info.address_u,
                          sampler_info.address_v,
                          vk::SamplerAddressMode::Repeat,
                          vk::CompareOp::Never)
                          .value();
        }

        textures.emplace_back(this->load_texture(Identifier::random(), image_info.format, image_info.extent, image_info.pixels, sampler));
    }

    std::vector<MaterialID> materials = {};
    for (auto &v : model_info->materials) {
        TextureID albedo_texture_id = TextureID::Invalid;
        TextureID normal_texture_id = TextureID::Invalid;
        TextureID emissive_texture_id = TextureID::Invalid;
        if (auto i = v.albedo_texture_index; i.has_value()) {
            albedo_texture_id = textures[i.value()];
        }
        if (auto i = v.normal_texture_index; i.has_value()) {
            normal_texture_id = textures[i.value()];
        }
        if (auto i = v.emissive_texture_index; i.has_value()) {
            emissive_texture_id = textures[i.value()];
        }

        materials.push_back(this->load_material(
            Identifier::random(),
            Material{
                .albedo_color = v.albedo_color,
                .emissive_color = v.emissive_color,
                .roughness_factor = v.roughness_factor,
                .metallic_factor = v.metallic_factor,
                .alpha_mode = v.alpha_mode,
                .alpha_cutoff = v.alpha_cutoff,
                .albedo_texture_id = albedo_texture_id,
                .normal_texture_id = normal_texture_id,
                .emissive_texture_id = emissive_texture_id,
            }));
    }

    std::vector<Model::Vertex> vertices = {};
    vertices.reserve(model_info->vertices.size());
    for (const auto &v : model_info->vertices) {
        vertices.push_back({
            .position = v.position,
            .uv_x = v.tex_coord_0.x,
            .normal = v.normal,
            .uv_y = v.tex_coord_0.y,
        });
    }

    std::vector<Model::Index> indices = {};
    indices.reserve(model_info->indices.size());
    for (const auto &v : model_info->indices) {
        indices.emplace_back(v);
    }

    std::vector<Model::Primitive> primitives = {};
    primitives.reserve(model_info->primitives.size());
    for (auto &v : model_info->primitives) {
        auto &primitive = primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset;
        primitive.vertex_count = v.vertex_count;
        primitive.index_offset = v.index_offset;
        primitive.index_count = v.index_count;
        primitive.material_id = materials[v.material_index];
    }

    std::vector<Model::Mesh> meshes = {};
    meshes.reserve(model_info->meshes.size());
    for (auto &v : model_info->meshes) {
        auto &mesh = meshes.emplace_back();
        mesh.name = v.name;
        mesh.primitive_indices = v.primitive_indices;
    }

    //  ── STAGING UPLOAD ──────────────────────────────────────────────────
    // TODO: Multiple duplication of CPU buffers, consider using CPU buffer
    // instead of vectors in the future. Currently, not a huge problem.
    // ─────────────────────────────────────────────────────────────────────
    auto transfer_man = impl->device.transfer_man();
    auto transfer_queue = impl->device.queue(vk::CommandType::Transfer);
    usize vertex_buffer_upload_size = vertices.size() * sizeof(Model::Vertex);
    auto vertex_buffer_gpu = Buffer::create(
                                 impl->device,
                                 vk::BufferUsage::Vertex | vk::BufferUsage::TransferDst,
                                 vertex_buffer_upload_size,
                                 vk::MemoryAllocationUsage::PreferDevice)
                                 .value()
                                 .set_name(std::format("{} Vertex Buffer", ident.sv()));
    usize index_buffer_upload_size = model_info->indices.size() * sizeof(Model::Index);
    auto index_buffer_gpu = Buffer::create(
                                impl->device,
                                vk::BufferUsage::Index | vk::BufferUsage::TransferDst,
                                index_buffer_upload_size,
                                vk::MemoryAllocationUsage::PreferDevice)
                                .value()
                                .set_name(std::format("{} Index Buffer", ident.sv()));

    {
        auto cmd_list = transfer_queue.begin();
        auto vertex_buffer_alloc = transfer_man.allocate(vertex_buffer_upload_size);
        std::memcpy(vertex_buffer_alloc->ptr, vertices.data(), vertex_buffer_upload_size);
        vk::BufferCopyRegion vertex_copy_region = { .src_offset = vertex_buffer_alloc->offset, .size = vertex_buffer_upload_size };
        cmd_list.copy_buffer_to_buffer(vertex_buffer_alloc->cpu_buffer_id, vertex_buffer_gpu.id(), vertex_copy_region);
        transfer_queue.end(cmd_list);
        transfer_queue.submit({}, transfer_man.semaphore());
        transfer_queue.wait();
        transfer_man.collect_garbage();
    }

    {
        auto cmd_list = transfer_queue.begin();
        auto index_buffer_alloc = transfer_man.allocate(index_buffer_upload_size);
        std::memcpy(index_buffer_alloc->ptr, indices.data(), index_buffer_upload_size);
        vk::BufferCopyRegion index_copy_region = { .src_offset = index_buffer_alloc->offset, .size = index_buffer_upload_size };
        cmd_list.copy_buffer_to_buffer(index_buffer_alloc->cpu_buffer_id, index_buffer_gpu.id(), index_copy_region);
        transfer_queue.end(cmd_list);
        transfer_queue.submit({}, transfer_man.semaphore());
        transfer_queue.wait();
        transfer_man.collect_garbage();
    }

    //   ──────────────────────────────────────────────────────────────────────
    auto *asset = this->register_asset(ident, "", AssetType::Model);
    if (!asset) {
        return ModelID::Invalid;
    }

    auto *model = this->get_model(asset->model_id);
    model->primitives = primitives;
    model->meshes = meshes;
    model->vertex_buffer = vertex_buffer_gpu;
    model->index_buffer = index_buffer_gpu;

    return asset->model_id;
}

auto AssetManager::load_material(const Identifier &ident, const Material &material_info) -> MaterialID {
    ZoneScoped;

    auto *asset = this->register_asset(ident, "", AssetType::Material);
    if (!asset) {
        return MaterialID::Invalid;
    }

    auto *material = this->get_material(asset->material_id);
    *material = material_info;

    auto *albedo_texture = this->get_texture(material_info.albedo_texture_id);
    auto *normal_texture = this->get_texture(material_info.normal_texture_id);
    auto *emissive_texture = this->get_texture(material_info.emissive_texture_id);

    // TODO: Invalid checkerboard image
    GPUMaterial gpu_material = {
        .albedo_color = material_info.albedo_color,
        .emissive_color = material_info.emissive_color,
        .roughness_factor = material_info.roughness_factor,
        .metallic_factor = material_info.metallic_factor,
        .alpha_mode = material_info.alpha_mode,
        .albedo_image = albedo_texture ? albedo_texture->sampled_image() : SampledImage(),
        .normal_image = normal_texture ? normal_texture->sampled_image() : SampledImage(),
        .emissive_image = emissive_texture ? emissive_texture->sampled_image() : SampledImage(),
    };

    u64 gpu_buffer_offset = std::to_underlying(asset->material_id) * sizeof(GPUMaterial);

    auto transfer_man = impl->device.transfer_man();
    auto transfer_queue = impl->device.queue(vk::CommandType::Transfer);
    auto cmd_list = transfer_queue.begin();
    auto gpu_material_alloc = transfer_man.allocate(sizeof(GPUMaterial));
    std::memcpy(gpu_material_alloc->ptr, &gpu_material, sizeof(GPUMaterial));

    vk::BufferCopyRegion copy_region = {
        .src_offset = gpu_material_alloc->offset,
        .dst_offset = gpu_buffer_offset,
        .size = sizeof(GPUMaterial),
    };
    cmd_list.copy_buffer_to_buffer(gpu_material_alloc->cpu_buffer_id, impl->material_buffer.id(), copy_region);

    transfer_queue.end(cmd_list);
    transfer_queue.submit({}, transfer_man.semaphore());
    transfer_queue.wait();

    return asset->material_id;
}

auto AssetManager::load_material(const Identifier &, const fs::path &) -> MaterialID {
    ZoneScoped;

    return MaterialID::Invalid;
}

auto AssetManager::get_asset(const Identifier &ident) -> Asset * {
    ZoneScoped;

    auto it = impl->registry.find(ident);
    if (it == impl->registry.end()) {
        return nullptr;
    }

    return &it->second;
}

auto AssetManager::get_model(const Identifier &ident) -> Model * {
    ZoneScoped;

    auto *asset = this->get_asset(ident);
    if (asset == nullptr) {
        return nullptr;
    }

    LS_EXPECT(asset->type == AssetType::Model);
    if (asset->type != AssetType::Model || asset->model_id == ModelID::Invalid) {
        return nullptr;
    }

    return &impl->models.get(asset->model_id);
}

auto AssetManager::get_model(ModelID model_id) -> Model * {
    ZoneScoped;

    if (model_id == ModelID::Invalid) {
        return nullptr;
    }

    return &impl->models.get(model_id);
}

auto AssetManager::get_texture(const Identifier &ident) -> Texture * {
    ZoneScoped;

    auto *asset = this->get_asset(ident);
    if (asset == nullptr) {
        return nullptr;
    }

    LS_EXPECT(asset->type == AssetType::Texture);
    if (asset->type != AssetType::Texture || asset->texture_id == TextureID::Invalid) {
        return nullptr;
    }

    return &impl->textures.get(asset->texture_id);
}

auto AssetManager::get_texture(TextureID texture_id) -> Texture * {
    ZoneScoped;

    if (texture_id == TextureID::Invalid) {
        return nullptr;
    }

    return &impl->textures.get(texture_id);
}

auto AssetManager::get_material(const Identifier &ident) -> Material * {
    ZoneScoped;

    auto *asset = this->get_asset(ident);
    if (asset == nullptr) {
        return nullptr;
    }

    LS_EXPECT(asset->type == AssetType::Material);
    if (asset->type != AssetType::Material || asset->material_id == MaterialID::Invalid) {
        return nullptr;
    }

    return &impl->materials.get(asset->material_id);
}

auto AssetManager::get_material(MaterialID material_id) -> Material * {
    ZoneScoped;

    if (material_id == MaterialID::Invalid) {
        return nullptr;
    }

    return &impl->materials.get(material_id);
}

}  // namespace lr
