#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Memory/Hasher.hh"
#include "Engine/Memory/PagedPool.hh"

#include "Engine/OS/File.hh"

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
    PagedPool<Scene, SceneID, 128_sz> scenes = {};

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
        case fnv64_c(".GLTF"):
            return AssetFileType::GLTF;
        case fnv64_c(".PNG"):
            return AssetFileType::PNG;
        case fnv64_c(".JPG"):
        case fnv64_c(".JPEG"):
            return AssetFileType::JPEG;
        case fnv64_c(".JSON"):
            return AssetFileType::JSON;
        case fnv64_c(".LRASSET"):
            return AssetFileType::Meta;
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

auto AssetManager::create_asset(AssetType type, const fs::path &path) -> UUID {
    ZoneScoped;

    auto uuid = UUID::generate_random();
    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        LOG_ERROR("Cannot create assert '{}'!", uuid.str());
        return UUID(nullptr);
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.type = type;
    asset.path = path;

    switch (type) {
        case AssetType::Model: {
            auto model_resource = impl->models.create();
            if (!model_resource) {
                return UUID(nullptr);
            }

            asset.model_id = model_resource->id;
            break;
        }
        case AssetType::Texture: {
            auto texture_resource = impl->textures.create();
            if (!texture_resource) {
                return UUID(nullptr);
            }

            asset.texture_id = texture_resource->id;
            break;
        }
        case AssetType::Scene: {
            auto scene_resource = impl->scenes.create();
            if (!scene_resource) {
                return UUID(nullptr);
            }

            asset.scene_id = scene_resource->id;
            break;
        }
        case AssetType::Material:
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
            LS_EXPECT(false);  // TODO: Other asset types
            return UUID(nullptr);
    }

    return asset.uuid;
}

auto AssetManager::init_new_model(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    auto *model = this->get_model(asset->model_id);
    auto gltf_model = GLTFModelInfo::parse(asset->path, false);
    if (!gltf_model.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", asset->path);
        return false;
    }

    for (usize index = 0; index < gltf_model->images.size(); index++) {
        auto texture_uuid = this->create_asset(AssetType::Texture);
        model->images.emplace_back(texture_uuid);
    }

    return true;
}

auto AssetManager::init_new_scene(const UUID &uuid, const std::string &name) -> bool {
    ZoneScoped;

    auto *scene = this->get_scene(uuid);
    if (scene->init(name)) {
        scene->create_editor_camera();
        return true;
    }

    return false;
}

auto AssetManager::import_asset(const fs::path &path) -> UUID {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

    LS_EXPECT(path.extension() == ".lrasset");
    if (!path.has_extension() || path.extension() != ".lrasset") {
        LOG_ERROR("'{}' is not a valid asset file. It must end with .lrasset");
        return UUID(nullptr);
    }

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return UUID(nullptr);
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return UUID(nullptr);
    }

    auto uuid_json = doc["uuid"].get_string();
    if (uuid_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `guid` is missing.");
        return UUID(nullptr);
    }

    auto type_json = doc["type"].get_number();
    if (type_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `type` is missing.");
        return UUID(nullptr);
    }

    auto asset_path = path;
    asset_path.replace_extension("");
    auto uuid = UUID::from_string(uuid_json.value()).value();
    auto type = static_cast<AssetType>(type_json.value().get_uint64());

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        // is it already in the registry?
        asset_it = impl->registry.find(uuid);
        if (asset_it == impl->registry.end()) {
            LOG_ERROR("Cannot insert assert '{}' into the registry!", uuid.str());
            return UUID(nullptr);
        }

        // Already inside registry, just return
        return uuid;
    }

    auto &asset = asset_it->second;
    asset.path = asset_path;
    asset.uuid = uuid;
    asset.type = type;

    switch (type) {
        case AssetType::Model: {
            auto model_resource = impl->models.create();
            if (!model_resource) {
                return UUID(nullptr);
            }

            asset.model_id = model_resource->id;
            auto *model = model_resource->self;

            auto images_json = doc["images"].get_array();
            for (auto image_json : images_json) {
                auto image_uuid = UUID::from_string(image_json.get_string().value());
                if (!image_uuid.has_value()) {
                    LOG_ERROR("Failed to import Model! An image with corrupt UUID.");
                    return UUID(nullptr);
                }

                model->images.emplace_back(image_uuid.value());
            }
        } break;
        case AssetType::Texture: {
            auto texture_resource = impl->textures.create();
            if (!texture_resource) {
                return UUID(nullptr);
            }

            asset.texture_id = texture_resource->id;
        } break;
        case AssetType::Scene: {
            auto scene_resource = impl->scenes.create();
            if (!scene_resource) {
                return UUID(nullptr);
            }

            asset.scene_id = scene_resource->id;
        } break;
        case AssetType::Material:
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
            LS_EXPECT(false);  // TODO: Other asset types
            return UUID(nullptr);
    }

    return uuid;
}

auto AssetManager::load_model(const UUID &uuid) -> bool {
    ZoneScoped;

    return true;
}

auto AssetManager::load_texture(
    const UUID &uuid, vuk::Format format, vuk::Extent3D extent, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    auto image = Image::create(
        *impl->device,
        format,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageType::e2D,
        extent,
        1,
        static_cast<u32>(glm::floor(glm::log2(static_cast<f32>(ls::max(extent.width, extent.height)))) + 1));
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

    auto *texture = this->get_texture(uuid);
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

auto AssetManager::load_texture(const UUID &uuid, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (!asset->path.has_extension()) {
        LOG_ERROR("Trying to load texture \"{}\" without a file extension.", asset->path);
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

    return this->load_texture(uuid, format, extent, pixels, sampler_info);
}

auto AssetManager::load_material(const UUID &uuid, const Material &material_info) -> bool {
    ZoneScoped;

    auto *material = this->get_material(uuid);
    *material = material_info;

    // auto *albedo_texture = this->get_texture(material_info.albedo_texture_id);
    // auto *normal_texture = this->get_texture(material_info.normal_texture_id);
    // auto *emissive_texture = this->get_texture(material_info.emissive_texture_id);
    //
    // // TODO: Invalid checkerboard image
    // GPUMaterial gpu_material = {
    //     .albedo_color = material_info.albedo_color,
    //     .emissive_color = material_info.emissive_color,
    //     .roughness_factor = material_info.roughness_factor,
    //     .metallic_factor = material_info.metallic_factor,
    //     .alpha_mode = 0,
    //     .albedo_image = albedo_texture ? albedo_texture->sampled_image() : SampledImage(),
    //     .normal_image = normal_texture ? normal_texture->sampled_image() : SampledImage(),
    //     .emissive_image = emissive_texture ? emissive_texture->sampled_image() : SampledImage(),
    // };
    //
    // u64 gpu_buffer_offset = std::to_underlying(asset->material_id) * sizeof(GPUMaterial);

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

auto AssetManager::load_material(const UUID &) -> bool {
    ZoneScoped;

    return true;
}

auto AssetManager::load_scene(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *scene = this->get_scene(uuid);
    if (!scene->init("unnamed_scene")) {
        return false;
    }

    auto *asset = this->get_asset(uuid);
    if (!scene->import_from_file(asset->path)) {
        return false;
    }

    // TODO: Load every asset this scene has
    return true;
}

auto AssetManager::unload_scene(const UUID &uuid) -> void {
    ZoneScoped;

    auto *scene = this->get_scene(uuid);
    scene->destroy();

    // TODO: Unload every asset this scene has
}

auto AssetManager::export_asset(const UUID &uuid, const fs::path &path) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    auto meta_path = path.string() + ".lrasset";

    JsonWriter json;
    json.begin_obj();
    json["uuid"] = asset->uuid.str();
    json["type"] = std::to_underlying(asset->type);

    switch (asset->type) {
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Model: {
            if (!this->export_model(asset->uuid, json, path)) {
                return false;
            }
        } break;
        case AssetType::Texture:
        case AssetType::Material:
        case AssetType::Font:
            break;
        case AssetType::Scene: {
            if (!this->export_scene(asset->uuid, json, path)) {
                return false;
            }

        } break;
    }

    json.end_obj();

    File file(meta_path, FileAccess::Write);
    if (!file) {
        return false;
    }

    file.write(json.stream.view().data(), json.stream.view().length());
    file.close();

    return true;
}

auto AssetManager::export_model(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool {
    ZoneScoped;

    auto *model = this->get_model(uuid);

    json["images"].begin_array();
    for (const auto &image : model->images) {
        json << image.str();
    }
    json.end_array();

    return true;
}

auto AssetManager::export_scene(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool {
    ZoneScoped;

    auto scene = this->get_scene(uuid);

    json["name"] = scene->get_name_sv();

    return scene->export_to_file(path);
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

auto AssetManager::get_scene(const UUID &uuid) -> Scene * {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (asset == nullptr) {
        return nullptr;
    }

    LS_EXPECT(asset->type == AssetType::Scene);
    if (asset->type != AssetType::Scene || asset->scene_id == SceneID::Invalid) {
        return nullptr;
    }

    return &impl->scenes.get(asset->scene_id);
}

auto AssetManager::get_scene(SceneID scene_id) -> Scene * {
    ZoneScoped;

    if (scene_id == SceneID::Invalid) {
        return nullptr;
    }

    return &impl->scenes.get(scene_id);
}

}  // namespace lr
