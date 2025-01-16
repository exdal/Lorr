#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Memory/Hasher.hh"
#include "Engine/Memory/PagedPool.hh"

#include "Engine/OS/File.hh"

#include "Engine/World/ECSModule/ComponentWrapper.hh"
#include "Engine/World/ECSModule/Core.hh"

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
    impl->device->set_name(impl->material_buffer, "GPU Material Buffer");

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
        case AssetType::Material: {
            auto material_resource = impl->materials.create();
            if (!material_resource) {
                return UUID(nullptr);
            }

            asset.material_id = material_resource->id;
        } break;
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
            LS_DEBUGBREAK();  // TODO: Other asset types
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

    for ([[maybe_unused]] const auto &v : gltf_model->images) {
        // First check for existing assets
        UUID texture_uuid = {};
        std::visit(
            match{
                [&](const std::vector<u8> &) {  //
                    texture_uuid = this->create_asset(AssetType::Texture, asset->path);
                },
                [&](const fs::path &path) {
                    memory::ScopedStack stack;
                    auto meta_file_path = stack.format("{}.lrasset", path);
                    if (!fs::exists(meta_file_path)) {
                        if (fs::exists(path)) {
                            texture_uuid = this->create_asset(AssetType::Texture, path);
                            this->export_asset(texture_uuid, path);
                        }
                        return;
                    }

                    texture_uuid = this->import_asset(meta_file_path);
                },
            },
            v.image_data);

        model->textures.emplace_back(texture_uuid);
    }

    for ([[maybe_unused]] const auto &v : gltf_model->materials) {
        auto material_uuid = this->create_asset(AssetType::Material);
        model->materials.emplace_back(material_uuid);
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

    if (!this->import_asset(uuid, type, asset_path)) {
        auto asset_it = impl->registry.find(uuid);
        if (asset_it != impl->registry.end()) {
            // Tried a reinsert, update asset info
            //
            auto &updating_asset = asset_it->second;
            updating_asset.path = asset_path;
            updating_asset.type = type;
            return uuid;
        }

        return UUID(nullptr);
    }

    switch (type) {
        case AssetType::Model: {
            auto *model = this->get_model(uuid);

            auto images_json = doc["textures"].get_array();
            for (auto image_json : images_json) {
                auto image_uuid = UUID::from_string(image_json.get_string().value());
                if (!image_uuid.has_value()) {
                    LOG_ERROR("Failed to import Model! An image with corrupt UUID.");
                    return UUID(nullptr);
                }

                this->import_asset(image_uuid.value(), AssetType::Texture, asset_path);
                model->textures.emplace_back(image_uuid.value());
            }

            auto materials_json = doc["materials"].get_array();
            for (auto material_json : materials_json) {
                auto material_uuid = UUID::from_string(material_json.get_string().value());
                if (!material_uuid.has_value()) {
                    LOG_ERROR("Failed to import Model! A material with corrupt UUID.");
                    return UUID(nullptr);
                }

                this->import_asset(material_uuid.value(), AssetType::Material, asset_path);
                model->materials.emplace_back(material_uuid.value());
            }
        }
        default:;
    }

    return uuid;
}

auto AssetManager::import_asset(const UUID &uuid, AssetType type, const fs::path &path) -> bool {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        return false;
    }

    auto &asset = asset_it->second;
    asset.path = path;
    asset.uuid = uuid;
    asset.type = type;

    switch (type) {
        case AssetType::Model: {
            auto model_resource = impl->models.create();
            if (!model_resource) {
                return false;
            }

            asset.model_id = model_resource->id;
        } break;
        case AssetType::Texture: {
            auto texture_resource = impl->textures.create();
            if (!texture_resource) {
                return false;
            }

            asset.texture_id = texture_resource->id;
        } break;
        case AssetType::Scene: {
            auto scene_resource = impl->scenes.create();
            if (!scene_resource) {
                return false;
            }

            asset.scene_id = scene_resource->id;
        } break;
        case AssetType::Material: {
            auto material_resource = impl->materials.create();
            if (!material_resource) {
                return false;
            }

            asset.material_id = material_resource->id;
        } break;
        default:
            LS_DEBUGBREAK();  // TODO: Other asset types
            return false;
    }

    return true;
}

auto AssetManager::load_asset(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    switch (asset->type) {
        case AssetType::Model: {
            return this->load_model(uuid);
        }
        case AssetType::Texture: {
            return this->load_texture(uuid);
        }
        default:;
    }

    return false;
}

auto AssetManager::load_model(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    auto *model = this->get_model(asset->model_id);
    if (!model) {
        LS_DEBUGBREAK();
        return false;
    }

    struct CallbackInfo {
        Device *device = nullptr;
        Model *model = nullptr;

        TransientBuffer vertex_buffer_cpu = {};
        TransientBuffer index_buffer_cpu = {};
    };
    auto on_buffer_sizes = [](void *user_data, usize vertex_count, usize index_count) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto &transfer_man = info->device->transfer_man();
        auto vertex_buffer_size = vertex_count * sizeof(GPUModel::Vertex);
        auto index_buffer_size = index_count * sizeof(GPUModel::Index);

        info->model->vertex_buffer = Buffer::create(*info->device, vertex_buffer_size).value();
        info->model->index_buffer = Buffer::create(*info->device, index_buffer_size).value();
        info->vertex_buffer_cpu = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, vertex_buffer_size);
        info->index_buffer_cpu = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, index_buffer_size);
    };
    auto on_access_index = [](void *user_data, u64 offset, u32 index) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto *ptr = info->index_buffer_cpu.host_ptr<u32>() + offset;
        *ptr = index;
    };
    auto on_access_position = [](void *user_data, u64 offset, glm::vec3 position) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto *ptr = info->vertex_buffer_cpu.host_ptr<GPUModel::Vertex>() + offset;
        ptr->position = position;
    };
    auto on_access_normal = [](void *user_data, u64 offset, glm::vec3 normal) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto *ptr = info->vertex_buffer_cpu.host_ptr<GPUModel::Vertex>() + offset;
        ptr->normal = normal;
    };
    auto on_access_texcoord = [](void *user_data, u64 offset, glm::vec2 texcoord) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto *ptr = info->vertex_buffer_cpu.host_ptr<GPUModel::Vertex>() + offset;
        ptr->tex_coord_0 = texcoord;
    };
    auto on_access_color = [](void *user_data, u64 offset, glm::vec4 color) {
        auto *info = static_cast<CallbackInfo *>(user_data);
        auto *ptr = info->vertex_buffer_cpu.host_ptr<GPUModel::Vertex>() + offset;
        ptr->color = glm::packUnorm4x8(color);
    };

    CallbackInfo callback_info = { .device = impl->device, .model = model };
    GLTFModelCallbacks callbacks = {
        .user_data = &callback_info,
        .on_buffer_sizes = on_buffer_sizes,
        .on_access_index = on_access_index,
        .on_access_position = on_access_position,
        .on_access_normal = on_access_normal,
        .on_access_texcoord = on_access_texcoord,
        .on_access_color = on_access_color,
    };
    auto gltf_model = GLTFModelInfo::parse(asset->path, true, callbacks);
    if (!gltf_model.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", asset->path);
        return false;
    }

    // TODO: We need to cache images, if theres one image with 2 sampler infos,
    // image gets copied twice. Add ability for one image to hold multiple
    // sampler infos.
    LS_EXPECT(model->textures.size() == gltf_model->textures.size());

    for (auto i = 0_sz; i < model->textures.size(); i++) {
        const auto &texture_uuid = model->textures[i];
        auto &gltf_texture = gltf_model->textures[i];
        auto &texture_data = gltf_model->images[gltf_texture.image_index.value()];
        auto &gltf_sampler = gltf_model->samplers[gltf_texture.sampler_index.value()];

        TextureSamplerInfo sampler_info = {
            .mag_filter = gltf_sampler.mag_filter,
            .min_filter = gltf_sampler.min_filter,
            .address_u = gltf_sampler.address_u,
            .address_v = gltf_sampler.address_v,
        };

        bool loaded = false;
        std::visit(
            match{
                [&](std::vector<u8> &pixels) {  //
                    loaded = this->load_texture(texture_uuid, pixels, sampler_info);
                },
                [&](const fs::path &) {  //
                    loaded = this->load_texture(texture_uuid, sampler_info);
                },

            },
            texture_data.image_data);
        if (!loaded) {
            LOG_ERROR("Failed to load texture {}!", texture_uuid.str());
            return false;
        }
    }

    LS_EXPECT(model->materials.size() == gltf_model->materials.size());

    for (auto i = 0_sz; i < model->materials.size(); i++) {
        const auto &material_uuid = model->materials[i];
        auto &gltf_material = gltf_model->materials[i];

        UUID albedo_texture_uuid = {};
        if (auto tex_idx = gltf_material.albedo_texture_index; tex_idx.has_value()) {
            albedo_texture_uuid = model->textures[tex_idx.value()];
        }

        UUID normal_texture_uuid = {};
        if (auto tex_idx = gltf_material.normal_texture_index; tex_idx.has_value()) {
            normal_texture_uuid = model->textures[tex_idx.value()];
        }

        UUID emissive_texture_uuid = {};
        if (auto tex_idx = gltf_material.emissive_texture_index; tex_idx.has_value()) {
            emissive_texture_uuid = model->textures[tex_idx.value()];
        }

        Material material_info = {
            .albedo_color = gltf_material.albedo_color,
            .emissive_color = gltf_material.emissive_color,
            .roughness_factor = gltf_material.roughness_factor,
            .alpha_cutoff = gltf_material.alpha_cutoff,
            .albedo_texture = albedo_texture_uuid,
            .normal_texture = normal_texture_uuid,
            .emissive_texture = emissive_texture_uuid,
        };
        this->load_material(material_uuid, material_info);
    }

    model->primitives.reserve(gltf_model->primitives.size());
    for (auto &v : gltf_model->primitives) {
        auto &primitive = model->primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset;
        primitive.vertex_count = v.vertex_count;
        primitive.index_offset = v.index_offset;
        primitive.index_count = v.index_count;
        primitive.material_index = v.material_index;
    }

    model->meshes.reserve(gltf_model->meshes.size());
    for (auto &v : gltf_model->meshes) {
        auto &mesh = model->meshes.emplace_back();
        mesh.name = v.name;
        mesh.primitive_indices = v.primitive_indices;
    }

    auto &transfer_man = impl->device->transfer_man();
    transfer_man.upload_staging(callback_info.vertex_buffer_cpu, model->vertex_buffer);
    transfer_man.upload_staging(callback_info.index_buffer_cpu, model->index_buffer);
    impl->device->set_name(model->vertex_buffer, "Model VB");
    impl->device->set_name(model->index_buffer, "Model IB");

    return true;
}

auto AssetManager::load_texture(const UUID &uuid, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    auto *texture = this->get_texture(asset->texture_id);
    if (!texture) {
        LS_DEBUGBREAK();
        return false;
    }

    vuk::Format format = {};
    vuk::Extent3D extent = {};
    std::vector<u8> raw_pixels = {};

    switch (this->to_asset_file_type(asset->path)) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            auto image = STBImageInfo::parse(pixels);
            if (!image.has_value()) {
                return false;
            }
            format = image->format;
            extent = image->extent;
            raw_pixels = std::move(image->data);
        } break;
        case AssetFileType::None:
        case AssetFileType::GLB:
        default:
            return false;
    }

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
        LS_DEBUGBREAK();
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
        LS_DEBUGBREAK();
        return false;
    }

    transfer_man.upload_staging(image_view.value(), raw_pixels);

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

    return this->load_texture(uuid, file_contents, sampler_info);
}

auto AssetManager::load_material(const UUID &uuid, const Material &material_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    auto *material = this->get_material(asset->material_id);
    *material = material_info;

    auto *albedo_texture = this->get_texture(material_info.albedo_texture);
    auto *normal_texture = this->get_texture(material_info.normal_texture);
    auto *emissive_texture = this->get_texture(material_info.emissive_texture);

    // TODO: Implement a checkerboard image for invalid textures
    u64 gpu_buffer_offset = std::to_underlying(asset->material_id) * sizeof(GPUMaterial);
    GPUMaterial gpu_material = {
        .albedo_color = material_info.albedo_color,
        .emissive_color = material_info.emissive_color,
        .roughness_factor = material_info.roughness_factor,
        .metallic_factor = material_info.metallic_factor,
        .alpha_mode = 0,
        .albedo_image = albedo_texture ? albedo_texture->sampled_image() : SampledImage(),
        .normal_image = normal_texture ? normal_texture->sampled_image() : SampledImage(),
        .emissive_image = emissive_texture ? emissive_texture->sampled_image() : SampledImage(),
    };

    auto &transfer_man = impl->device->transfer_man();
    transfer_man.upload_staging(
        impl->material_buffer, ls::span(ls::bit_cast<u8 *>(&gpu_material), sizeof(GPUMaterial)), gpu_buffer_offset, sizeof(GPUMaterial));

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

    ankerl::unordered_dense::set<UUID> cached_assets = {};
    scene->get_root().children([&](flecs::entity e) {
        e.each([&](flecs::id component_id) {
            auto ecs_world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            ECS::ComponentWrapper component(e, component_id);
            if (!component.has_component()) {
                return;
            }

            component.for_each([&](usize, std::string_view, ECS::ComponentWrapper::Member &member) {
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](UUID *v) { cached_assets.emplace(*v); },
                    },
                    member);
            });
        });
    });

    for (const auto &cached_uuid : cached_assets) {
        this->load_asset(cached_uuid);
    }

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
        case AssetType::Texture: {
            if (!this->export_texture(asset->uuid, json, path)) {
                return false;
            }
        } break;
        case AssetType::Model: {
            if (!this->export_model(asset->uuid, json, path)) {
                return false;
            }
        } break;
        case AssetType::Scene: {
            if (!this->export_scene(asset->uuid, json, path)) {
                return false;
            }
        } break;
        default:
            return false;
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

auto AssetManager::export_texture(const UUID &, JsonWriter &, const fs::path &) -> bool {
    ZoneScoped;

    // TODO: Add additional sampler information
    // auto *texture = this->get_texture(uuid);
    return true;
}

auto AssetManager::export_model(const UUID &uuid, JsonWriter &json, const fs::path &) -> bool {
    ZoneScoped;

    auto *model = this->get_model(uuid);

    json["textures"].begin_array();
    for (const auto &image : model->textures) {
        json << image.str();
    }
    json.end_array();

    json["materials"].begin_array();
    for (const auto &material : model->materials) {
        json << material.str();
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
