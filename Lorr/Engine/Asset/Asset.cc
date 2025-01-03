#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Memory/Hasher.hh"
#include "Engine/Memory/PagedPool.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"

#include <simdjson.h>

namespace lr {
template<>
struct Handle<AssetManager>::Impl {
    Device *device = nullptr;
    fs::path root_path = fs::current_path();
    AssetRegistry registry = {};

    PagedPool<Model, ModelID> models = {};
    PagedPool<Texture, TextureID> textures = {};
    PagedPool<Material, MaterialID, 1024_sz> materials = {};
    PagedPool<Scene::Impl *, SceneID, 128_sz> scenes = {};

    Buffer material_buffer = {};
};

auto AssetManager::create(Device *device) -> AssetManager {
    ZoneScoped;

    auto impl = new Impl;
    auto self = AssetManager(impl);

    impl->device = device;
    impl->material_buffer = Buffer::create(*device, impl->materials.max_resources() * sizeof(GPUMaterial)).value();

    impl->root_path = fs::current_path();

    return self;
}

auto AssetManager::destroy() -> void {
    ZoneScoped;
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
        case fnv64_c(".JSON"):
            return AssetFileType::JSON;
        default:
            return AssetFileType::None;
    }
}

auto AssetManager::material_buffer() const -> Buffer & {
    return impl->material_buffer;
}

auto AssetManager::registry() const -> const AssetRegistry & {
    return impl->registry;
}

auto AssetManager::add_asset(const fs::path &path) -> Asset * {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

    LS_EXPECT(path.extension() == ".lrasset");
    if (!path.has_extension() || path.extension() != ".lrasset") {
        LOG_ERROR("'{}' is not a valid asset file. It must end with .lrasset");
        return nullptr;
    }

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return nullptr;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return nullptr;
    }

    auto uuid_json = doc["uuid"].get_string();
    if (uuid_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `guid` is missing.");
        return nullptr;
    }

    auto type_json = doc["type"].get_number();
    if (type_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `type` is missing.");
        return nullptr;
    }

    auto asset_path = path;
    asset_path.replace_extension("");
    auto uuid = UUID::from_string(uuid_json.value()).value();
    auto type = static_cast<AssetType>(type_json.value().get_uint64());

    return this->register_asset(asset_path, uuid, type);
}

auto AssetManager::register_asset(const fs::path &path, UUID &uuid, AssetType type) -> Asset * {
    ZoneScoped;

    switch (type) {
        case AssetType::Model: {
            return this->register_model(path, uuid);
        }
        case AssetType::Texture: {
            return this->register_texture(path, uuid);
        }
        case AssetType::Material: {
            return this->register_material(path, uuid);
        }
        case AssetType::Scene: {
            return this->register_scene(path, uuid);
        }
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
            LS_EXPECT(false);  // TODO: Other asset types
            return nullptr;
    }
}

auto AssetManager::register_model(const fs::path &path, UUID &uuid) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        LOG_ERROR("Cannot register new model!");
        return {};
    }

    auto model_result = impl->models.create();
    if (!model_result.has_value()) {
        LOG_ERROR("Failed to create new model, out of pool space.");
        return {};
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = path;
    asset.type = AssetType::Model;
    asset.model_id = model_result->id;

    return &asset;
}

auto AssetManager::register_texture(const fs::path &path, UUID &uuid) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        LOG_ERROR("Cannot register new texture!");
        return {};
    }

    auto texture_result = impl->textures.create();
    if (!texture_result.has_value()) {
        LOG_ERROR("Failed to create new texture, out of pool space.");
        return {};
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = path;
    asset.type = AssetType::Texture;
    asset.texture_id = texture_result->id;

    return &asset;
}

auto AssetManager::register_material(const fs::path &path, UUID &uuid) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        LOG_ERROR("Cannot register new scene!");
        return {};
    }

    auto material_result = impl->materials.create();
    if (!material_result.has_value()) {
        LOG_ERROR("Failed to create new material, out of pool space.");
        return {};
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = path;
    asset.type = AssetType::Material;
    asset.material_id = material_result->id;

    return &asset;
}

auto AssetManager::register_scene(const fs::path &path, UUID &uuid) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        LOG_ERROR("Cannot register new scene!");
        return {};
    }

    auto scene_result = impl->scenes.create();
    if (!scene_result.has_value()) {
        LOG_ERROR("Failed to create new scene, out of pool space.");
        return {};
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = path;
    asset.type = AssetType::Scene;
    asset.scene_id = scene_result->id;

    return &asset;
}

auto AssetManager::create_scene(const std::string &name, const fs::path &path) -> Asset * {
    ZoneScoped;

    auto &app = Application::get();
    auto uuid = UUID::generate_random();
    auto asset = this->register_scene(path, uuid);

    auto scene = Scene::create(name, &app.world);
    scene->create_editor_camera();
    impl->scenes.get(asset->scene_id) = scene->impl;

    this->export_scene(asset, asset->path);
    this->write_asset_meta(asset, uuid);

    return asset;
}

auto AssetManager::write_asset_meta(Asset *asset, const UUID &uuid) -> void {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["uuid"] = uuid.str();
    json["type"] = static_cast<u32>(asset->type);
    json.end_obj();

    auto path = asset->path.replace_extension(asset->path.extension().string() + ".lrasset");
    File file(path, FileAccess::Write);
    file.write(json.stream.view().data(), json.stream.view().length());
    file.close();
}

auto AssetManager::import_texture(const fs::path &path) -> Asset * {
    ZoneScoped;

    auto uuid = UUID::generate_random();
    auto asset = this->register_texture(path, uuid);

    LS_EXPECT(this->load_texture(asset));

    return asset;
}

auto AssetManager::load_model(Asset *asset) -> bool {
    ZoneScoped;

    auto model_info = GLTFModelInfo::parse(asset->path);
    if (!model_info.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", asset->path);
        return false;
    }

    std::vector<TextureID> textures = {};
    textures.reserve(model_info->textures.size());
    for (auto &v : model_info->textures) {
        if (!v.image_index.has_value()) {
            LOG_ERROR("Model {} has invalid image.", asset->path);
            return false;
        }

        auto &image_info = model_info->images[v.image_index.value()];
        TextureSamplerInfo sampler_info = {};
        if (v.sampler_index.has_value()) {
            auto &gltf_sampler_info = model_info->samplers[v.sampler_index.value()];
            sampler_info.address_u = gltf_sampler_info.address_u;
            sampler_info.address_v = gltf_sampler_info.address_v;
            sampler_info.min_filter = gltf_sampler_info.min_filter;
            sampler_info.mag_filter = gltf_sampler_info.mag_filter;
        }

        // textures.emplace_back(
        //     this->load_texture(UUID::generate_random(), image_info.format, image_info.extent, image_info.pixels, sampler_info));
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

        // materials.push_back(this->load_material(
        //     UUID::generate_random(),
        //     Material{
        //         .albedo_color = v.albedo_color,
        //         .emissive_color = v.emissive_color,
        //         .roughness_factor = v.roughness_factor,
        //         .metallic_factor = v.metallic_factor,
        //         .alpha_mode = v.alpha_mode,
        //         .alpha_cutoff = v.alpha_cutoff,
        //         .albedo_texture_id = albedo_texture_id,
        //         .normal_texture_id = normal_texture_id,
        //         .emissive_texture_id = emissive_texture_id,
        //     }));
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

    auto &transfer_man = impl->device->transfer_man();
    usize vertex_buffer_upload_size = vertices.size() * sizeof(Model::Vertex);
    usize index_buffer_upload_size = model_info->indices.size() * sizeof(Model::Index);
    auto vertex_buffer = Buffer::create(*impl->device, vertex_buffer_upload_size).value();
    auto index_buffer = Buffer::create(*impl->device, index_buffer_upload_size).value();

    transfer_man.upload_staging(vertex_buffer, { reinterpret_cast<u8 *>(vertices.data()), vertex_buffer_upload_size });
    transfer_man.upload_staging(index_buffer, { reinterpret_cast<u8 *>(indices.data()), index_buffer_upload_size });

    //   ──────────────────────────────────────────────────────────────────────
    auto *model = this->get_model(asset->model_id);
    model->primitives = primitives;
    model->meshes = meshes;
    model->vertex_buffer = vertex_buffer;
    model->index_buffer = index_buffer;

    return true;
}

auto AssetManager::load_texture(
    Asset *asset, vuk::Format format, vuk::Extent3D extent, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    auto image = Image::create(*impl->device, format, vuk::ImageUsageFlagBits::eSampled, vuk::ImageType::e2D, extent);
    // static_cast<u32>(glm::floor(glm::log2(static_cast<f32>(ls::max(extent.width, extent.height)))) + 1));
    if (!image.has_value()) {
        return false;
    }

    auto image_view = ImageView::create(
        *impl->device,
        image.value(),
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageViewType::e2D,
        {
            .aspectMask = vuk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = image->mip_count(),
            .baseArrayLayer = 0,
            .layerCount = image->slice_count(),
        });
    if (!image_view.has_value()) {
        return false;
    }

    transfer_man.upload_staging(image_view.value(), pixels);

    auto *texture = this->get_texture(asset->texture_id);
    texture->image = image.value();
    texture->image_view = image_view.value();
    texture->sampler =  //
        Sampler::create(
            *impl->device,
            sampler_info.min_filter,
            sampler_info.mag_filter,
            vuk::SamplerMipmapMode::eLinear,
            sampler_info.address_u,
            sampler_info.address_v,
            vuk::SamplerAddressMode::eRepeat,
            vuk::CompareOp::eNever)
            .value();

    return true;
}

auto AssetManager::load_texture(Asset *asset, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    if (!asset->path.has_extension()) {
        LOG_ERROR("Trying to load texture \"{}\" file without an extension.", asset->path);
        return false;
    }

    auto file_contents = File::to_bytes(asset->path);
    if (file_contents.empty()) {
        LOG_ERROR("Cannot read texture file.");
        return false;
    }

    vuk::Format format = {};
    vuk::Extent3D extent = {};
    std::vector<u8> pixels = {};

    switch (this->to_asset_file_type(asset->path)) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            auto image = STBImageInfo::parse(file_contents);
            if (!image.has_value()) {
                return false;
            }
            format = image->format;
            extent = image->extent;
            pixels = std::move(image->data);
            break;
        }
        case AssetFileType::None:
        case AssetFileType::GLB:
        default:
            return false;
    }

    return this->load_texture(asset, format, extent, pixels, sampler_info);
}

auto AssetManager::load_material(Asset *asset, const Material &material_info) -> bool {
    ZoneScoped;

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

    // auto transfer_man = impl->device.transfer_man();
    // auto transfer_queue = impl->device.queue(vk::CommandType::Transfer);
    // auto cmd_list = transfer_queue.begin();
    // auto gpu_material_alloc = transfer_man.allocate(sizeof(GPUMaterial));
    // std::memcpy(gpu_material_alloc->ptr, &gpu_material, sizeof(GPUMaterial));
    //
    // vk::BufferCopyRegion copy_region = {
    //     .src_offset = gpu_material_alloc->offset,
    //     .dst_offset = gpu_buffer_offset,
    //     .size = sizeof(GPUMaterial),
    // };
    // cmd_list.copy_buffer_to_buffer(gpu_material_alloc->cpu_buffer_id, impl->material_buffer.id(), copy_region);
    //
    // transfer_queue.end(cmd_list);
    // transfer_queue.submit({}, transfer_man.semaphore());
    // transfer_queue.wait();

    return true;
}

auto AssetManager::load_material(Asset *) -> bool {
    ZoneScoped;

    return true;
}

auto AssetManager::load_scene(Asset *asset) -> bool {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;

    auto scene = Scene::create_from_file(asset->path, &world).value();
    impl->scenes.get(asset->scene_id) = scene.impl;

    return true;
}

auto AssetManager::export_scene(Asset *asset, const fs::path &path) -> bool {
    ZoneScoped;

    auto scene = this->get_scene(asset->scene_id);
    LS_EXPECT(scene);
    scene.export_to_file(path);

    return true;
}

auto AssetManager::get_asset(const UUID &uuid) -> Asset * {
    ZoneScoped;

    auto it = impl->registry.find(uuid);
    if (it == impl->registry.end()) {
        return nullptr;
    }

    return &it->second;
}

auto AssetManager::get_model(const UUID &uuid) -> Model * {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
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

auto AssetManager::get_texture(const UUID &uuid) -> Texture * {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
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

auto AssetManager::get_material(const UUID &uuid) -> Material * {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
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

auto AssetManager::get_scene(const UUID &uuid) -> Scene {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (asset == nullptr) {
        return Scene(nullptr);
    }

    LS_EXPECT(asset->type == AssetType::Scene);
    if (asset->type != AssetType::Scene || asset->scene_id == SceneID::Invalid) {
        return Scene(nullptr);
    }

    return Scene(impl->scenes.get(asset->scene_id));
}

auto AssetManager::get_scene(SceneID scene_id) -> Scene {
    ZoneScoped;

    if (scene_id == SceneID::Invalid) {
        return Scene(nullptr);
    }

    return Scene(impl->scenes.get(scene_id));
}

}  // namespace lr
