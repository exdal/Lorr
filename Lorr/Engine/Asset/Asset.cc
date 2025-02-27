#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Memory/Hasher.hh"

#include "Engine/OS/File.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include <meshoptimizer.h>
#include <simdjson.h>

namespace lr {
template<>
struct Handle<AssetManager>::Impl {
    Device *device = nullptr;
    fs::path root_path = fs::current_path();
    AssetRegistry registry = {};

    SlotMap<Model, ModelID> models = {};
    SlotMap<Texture, TextureID> textures = {};
    SlotMap<Material, MaterialID> materials = {};
    SlotMap<Scene, SceneID> scenes = {};

    Buffer material_buffer = {};
};

auto AssetManager::create(Device *device) -> AssetManager {
    ZoneScoped;

    auto impl = new Impl;
    auto self = AssetManager(impl);

    impl->device = device;
    impl->root_path = fs::current_path();
    impl->material_buffer = Buffer::create(*device, 1024 * sizeof(GPU::Material)).value();
    impl->device->set_name(impl->material_buffer, "GPU Material Buffer");

    return self;
}

auto AssetManager::destroy() -> void {
    ZoneScoped;

    delete impl;
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

auto AssetManager::to_asset_type_sv(AssetType type) -> std::string_view {
    ZoneScoped;

    switch (type) {
        case AssetType::None:
            return "None";
        case AssetType::Shader:
            return "Shader";
        case AssetType::Model:
            return "Model";
        case AssetType::Texture:
            return "Texture";
        case AssetType::Material:
            return "Material";
        case AssetType::Font:
            return "Font";
        case AssetType::Scene:
            return "Scene";
    }
}

auto AssetManager::material_buffer() const -> BufferID {
    return impl->material_buffer.id();
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

    return asset.uuid;
}

auto AssetManager::init_new_scene(const UUID &uuid, const std::string &name) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    asset->scene_id = impl->scenes.create_slot();
    auto *scene = impl->scenes.slot(asset->scene_id);
    if (!scene->init(name)) {
        return false;
    }

    scene->create_editor_camera();

    asset->acquire_ref();
    return true;
}

auto AssetManager::import_asset(const fs::path &path) -> UUID {
    ZoneScoped;
    memory::ScopedStack stack;

    auto meta_file_path = stack.format("{}.lrasset", path);
    auto meta_exists = fs::exists(meta_file_path);
    if (meta_exists) {
        return this->register_asset(meta_file_path);
    }

    switch (this->to_asset_file_type(path)) {
        case AssetFileType::Meta: {
            return this->register_asset(path);
        }
        case AssetFileType::GLTF:
        case AssetFileType::GLB: {
            auto uuid = this->create_asset(AssetType::Model, path);
            if (!uuid) {
                return UUID(nullptr);
            }

            auto gltf_model = GLTFModelInfo::parse_info(path);
            Model model = {};
            for (auto &v : gltf_model->textures) {
                auto &image = gltf_model->images[v.image_index.value()];
                UUID texture_uuid = {};
                std::visit(
                    ls::match{
                        [&](const std::vector<u8> &) {  //
                            texture_uuid = this->create_asset(AssetType::Texture, path);
                        },
                        [&](const fs::path &image_path) {  //
                            texture_uuid = this->import_asset(image_path);
                        },
                    },
                    image.image_data);

                model.textures.emplace_back(texture_uuid);
            }

            for ([[maybe_unused]] const auto &v : gltf_model->materials) {
                auto material_uuid = this->create_asset(AssetType::Material);
                model.materials.emplace_back(material_uuid);
            }

            JsonWriter json;
            this->begin_asset_meta(json, uuid, AssetType::Model);
            this->write_model_asset_meta(json, &model);
            if (!this->end_asset_meta(json, path)) {
                return UUID(nullptr);
            }

            return uuid;
        }
        case AssetFileType::JPEG:
        case AssetFileType::PNG: {
            auto uuid = this->create_asset(AssetType::Texture, path);
            if (!uuid) {
                return UUID(nullptr);
            }

            JsonWriter json;
            this->begin_asset_meta(json, uuid, AssetType::Texture);
            // this->write_texture_asset_meta(json, &texture);
            if (!this->end_asset_meta(json, path)) {
                return UUID(nullptr);
            }

            return uuid;
        }

        default:;
    }

    return UUID(nullptr);
}

struct AssetMetaFile {
    simdjson::padded_string contents;
    simdjson::ondemand::parser parser;
    simdjson::simdjson_result<simdjson::ondemand::document> doc;
};

auto read_meta_file(const fs::path &path) -> std::unique_ptr<AssetMetaFile> {
    ZoneScoped;

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

    auto result = std::make_unique<AssetMetaFile>();
    result->contents = simdjson::padded_string(file.size);
    file.read(result->contents.data(), file.size);
    result->doc = result->parser.iterate(result->contents);
    if (result->doc.error()) {
        LOG_ERROR("Failed to parse asset meta file! {}", simdjson::error_message(result->doc.error()));
        return nullptr;
    }

    return result;
}

auto AssetManager::register_asset(const fs::path &path) -> UUID {
    ZoneScoped;
    memory::ScopedStack stack;

    auto meta_json = read_meta_file(path);
    if (!meta_json) {
        return UUID(nullptr);
    }

    auto uuid_json = meta_json->doc["uuid"].get_string();
    if (uuid_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `uuid` is missing.");
        return UUID(nullptr);
    }

    auto type_json = meta_json->doc["type"].get_number();
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

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = asset_path;
    asset.type = type;

    return uuid;
}

auto AssetManager::register_asset(const UUID &uuid, AssetType type, const fs::path &path) -> bool {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        return false;
    }

    auto &asset = asset_it->second;
    asset.uuid = uuid;
    asset.path = path;
    asset.type = type;

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
        case AssetType::Scene: {
            return this->load_scene(uuid);
        }
        default:;
    }

    return false;
}

auto AssetManager::unload_asset(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    switch (asset->type) {
        case AssetType::Model: {
            this->unload_model(uuid);
        } break;
        case AssetType::Texture: {
            this->unload_texture(uuid);
        } break;
        case AssetType::Scene: {
            this->unload_scene(uuid);
        } break;
        default:;
    }
}

auto AssetManager::load_model(const UUID &uuid) -> bool {
    ZoneScoped;
    memory::ScopedStack stack;

    auto *asset = this->get_asset(uuid);
    if (asset->is_loaded()) {
        asset->acquire_ref();
        return true;
    }

    asset->model_id = impl->models.create_slot();
    auto *model = impl->models.slot(asset->model_id);

    fs::path meta_path = asset->path.string() + ".lrasset";
    auto meta_json = read_meta_file(meta_path);
    if (!meta_json) {
        LOG_ERROR("Model assets require proper meta file.");
        return false;
    }

    struct GLTFCallbacks {
        Model *model = nullptr;

        // Per mesh data
        std::vector<glm::vec3> vertex_positions = {};
        std::vector<glm::vec3> vertex_normals = {};
        std::vector<glm::vec2> vertex_texcoords = {};
        std::vector<Model::Index> indices = {};
    };
    auto on_new_node = [](void *user_data,
                          u32 mesh_index,
                          [[maybe_unused]] u32 primitive_count,
                          u32 vertex_count,
                          u32 index_count,
                          [[maybe_unused]] glm::mat4 transform) {
        auto *info = static_cast<GLTFCallbacks *>(user_data);

        if (info->model->meshes.size() <= mesh_index) {
            info->model->meshes.resize(mesh_index + 1);
        }

        info->vertex_positions.resize(info->vertex_positions.size() + vertex_count);
        info->vertex_normals.resize(info->vertex_normals.size() + vertex_count);
        info->vertex_texcoords.resize(info->vertex_texcoords.size() + vertex_count);
        info->indices.resize(info->indices.size() + index_count);
    };
    auto on_new_primitive =
        [](void *user_data, u32 mesh_index, u32 material_index, u32 vertex_offset, u32 vertex_count, u32 index_offset, u32 index_count) {
            auto *info = static_cast<GLTFCallbacks *>(user_data);
            auto &mesh = info->model->meshes[mesh_index];
            mesh.primitive_indices.push_back(info->model->primitives.size());
            auto &primitive = info->model->primitives.emplace_back();
            primitive.material_index = material_index;
            primitive.vertex_offset = vertex_offset;
            primitive.vertex_count = vertex_count;
            primitive.index_offset = index_offset;
            primitive.index_count = index_count;
        };
    auto on_access_index = [](void *user_data, u32, u64 offset, u32 index) {
        auto *info = static_cast<GLTFCallbacks *>(user_data);
        info->indices[offset] = index;
    };
    auto on_access_position = [](void *user_data, u32, u64 offset, glm::vec3 position) {
        auto *info = static_cast<GLTFCallbacks *>(user_data);
        info->vertex_positions[offset] = position;
    };
    auto on_access_normal = [](void *user_data, u32, u64 offset, glm::vec3 normal) {
        auto *info = static_cast<GLTFCallbacks *>(user_data);
        info->vertex_normals[offset] = normal;
    };
    auto on_access_texcoord = [](void *user_data, u32, u64 offset, glm::vec2 texcoord) {
        auto *info = static_cast<GLTFCallbacks *>(user_data);
        info->vertex_texcoords[offset] = texcoord;
    };

    GLTFCallbacks gltf_callbacks = { .model = model };
    auto gltf_model = GLTFModelInfo::parse(
        asset->path,
        { .user_data = &gltf_callbacks,
          .on_new_node = on_new_node,
          .on_new_primitive = on_new_primitive,
          .on_access_index = on_access_index,
          .on_access_position = on_access_position,
          .on_access_normal = on_access_normal,
          .on_access_texcoord = on_access_texcoord });
    if (!gltf_model.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", asset->path);
        return false;
    }

    auto images_json = meta_json->doc["textures"].get_array();
    for (auto image_json : images_json) {
        auto image_uuid = UUID::from_string(image_json.get_string().value());
        if (!image_uuid.has_value()) {
            LOG_ERROR("Failed to import Model! An image with corrupt UUID.");
            return false;
        }

        this->register_asset(image_uuid.value(), AssetType::Texture, asset->path);
        model->textures.emplace_back(image_uuid.value());
    }

    auto materials_json = meta_json->doc["materials"].get_array();
    for (auto material_json : materials_json) {
        auto material_uuid = UUID::from_string(material_json.get_string().value());
        if (!material_uuid.has_value()) {
            LOG_ERROR("Failed to import Model! A material with corrupt UUID.");
            return false;
        }

        this->register_asset(material_uuid.value(), AssetType::Material, asset->path);
        model->materials.emplace_back(material_uuid.value());
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
            ls::match{
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

    //  ── MESH PROCESSING ─────────────────────────────────────────────────
    std::vector<glm::vec3> model_vertex_positions = {};
    std::vector<u32> model_indices = {};

    std::vector<GPU::Meshlet> model_meshlets = {};
    std::vector<GPU::MeshletBounds> model_meshlet_bounds = {};
    std::vector<u8> model_local_triangle_indices = {};

    for (const auto &mesh : model->meshes) {
        for (auto primitive_index : mesh.primitive_indices) {
            ZoneScopedN("GPU Meshlet Generation");

            const auto &primitive = model->primitives[primitive_index];
            auto vertex_offset = model_vertex_positions.size();
            auto index_offset = model_indices.size();
            auto triangle_offset = model_local_triangle_indices.size();

            auto raw_vertex_positions = ls::span(gltf_callbacks.vertex_positions.data() + primitive.vertex_offset, primitive.vertex_count);
            auto raw_indices = ls::span(gltf_callbacks.indices.data() + primitive.index_offset, primitive.index_count);

            auto meshlets = std::vector<GPU::Meshlet>();
            auto meshlet_bounds = std::vector<GPU::MeshletBounds>();
            auto meshlet_indices = std::vector<u32>();
            auto local_triangle_indices = std::vector<u8>();
            {
                ZoneScopedN("Build Meshlets");
                // Worst case count
                auto max_meshlets = meshopt_buildMeshletsBound(  //
                    raw_indices.size(),
                    Model::MAX_MESHLET_INDICES,
                    Model::MAX_MESHLET_PRIMITIVES);
                auto raw_meshlets = std::vector<meshopt_Meshlet>(max_meshlets);
                meshlet_indices.resize(max_meshlets * Model::MAX_MESHLET_INDICES);
                local_triangle_indices.resize(max_meshlets * Model::MAX_MESHLET_PRIMITIVES * 3);
                auto meshlet_count = meshopt_buildMeshlets(  //
                    raw_meshlets.data(),
                    meshlet_indices.data(),
                    local_triangle_indices.data(),
                    raw_indices.data(),
                    raw_indices.size(),
                    reinterpret_cast<f32 *>(raw_vertex_positions.data()),
                    raw_vertex_positions.size(),
                    sizeof(glm::vec3),
                    Model::MAX_MESHLET_INDICES,
                    Model::MAX_MESHLET_PRIMITIVES,
                    0.0);

                // Trim meshlets from worst case to current case
                raw_meshlets.resize(meshlet_count);
                meshlets.resize(meshlet_count);
                meshlet_bounds.resize(meshlet_count);
                const auto &last_meshlet = raw_meshlets[meshlet_count - 1];
                meshlet_indices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
                local_triangle_indices.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3_u32));

                for (const auto &[raw_meshlet, meshlet, meshlet_aabb] : std::views::zip(raw_meshlets, meshlets, meshlet_bounds)) {
                    auto meshlet_bb_min = glm::vec3(std::numeric_limits<f32>::max());
                    auto meshlet_bb_max = glm::vec3(std::numeric_limits<f32>::lowest());
                    for (u32 i = 0; i < raw_meshlet.triangle_count * 3; i++) {
                        const auto &tri_pos = raw_vertex_positions
                            [meshlet_indices[raw_meshlet.vertex_offset + local_triangle_indices[raw_meshlet.triangle_offset + i]]];
                        meshlet_bb_min = glm::min(meshlet_bb_min, tri_pos);
                        meshlet_bb_max = glm::max(meshlet_bb_max, tri_pos);
                    }

                    meshlet.vertex_offset = vertex_offset;
                    meshlet.index_offset = index_offset + raw_meshlet.vertex_offset;
                    meshlet.triangle_offset = triangle_offset + raw_meshlet.triangle_offset;
                    meshlet.triangle_count = raw_meshlet.triangle_count;
                    meshlet_aabb.aabb_min = meshlet_bb_min;
                    meshlet_aabb.aabb_max = meshlet_bb_max;
                }
            }

            std::ranges::move(raw_vertex_positions, std::back_inserter(model_vertex_positions));
            std::ranges::move(meshlet_indices, std::back_inserter(model_indices));
            std::ranges::move(meshlets, std::back_inserter(model_meshlets));
            std::ranges::move(meshlet_bounds, std::back_inserter(model_meshlet_bounds));
            std::ranges::move(local_triangle_indices, std::back_inserter(model_local_triangle_indices));
        }
    }

    model->meshlet_count = model_meshlets.size();

    auto &transfer_man = impl->device->transfer_man();
    model->vertex_positions = Buffer::create(*impl->device, ls::size_bytes(model_vertex_positions)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_vertex_positions), model->vertex_positions));

    model->indices = Buffer::create(*impl->device, ls::size_bytes(model_indices)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_indices), model->indices));

    model->meshlets = Buffer::create(*impl->device, ls::size_bytes(model_meshlets)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_meshlets), model->meshlets));

    model->meshlet_bounds = Buffer::create(*impl->device, ls::size_bytes(model_meshlet_bounds)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_meshlet_bounds), model->meshlet_bounds));

    model->local_triangle_indices = Buffer::create(*impl->device, ls::size_bytes(model_local_triangle_indices)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_local_triangle_indices), model->local_triangle_indices));

    asset->acquire_ref();
    return true;
}

auto AssetManager::unload_model(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    if (!(asset->is_loaded() && asset->release_ref())) {
        return;
    }

    auto *model = this->get_model(asset->model_id);
    model->meshes.clear();

    for (auto &v : model->materials) {
        this->unload_material(v);
    }

    for (auto &v : model->textures) {
        this->unload_texture(v);
    }

    impl->models.destroy_slot(asset->model_id);
    asset->model_id = ModelID::Invalid;
}

auto AssetManager::load_texture(const UUID &uuid, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    asset->texture_id = impl->textures.create_slot();
    auto *texture = impl->textures.slot(asset->texture_id);

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
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferSrc,
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
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferSrc,
        vuk::ImageViewType::e2D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor,
          .baseMipLevel = 0,
          .levelCount = image->mip_count(),
          .baseArrayLayer = 0,
          .layerCount = image->slice_count() });
    if (!image_view.has_value()) {
        LS_DEBUGBREAK();
        return false;
    }

    auto attachment = transfer_man.upload_staging(image_view.value(), raw_pixels.data(), ls::size_bytes(raw_pixels))
                          .as_released(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
    attachment->layout = vuk::ImageLayout::eReadOnlyOptimal;
    transfer_man.wait_on(std::move(attachment));

    auto rel_path = fs::relative(asset->path, impl->root_path);
    impl->device->set_name(image.value(), std::format("{} Image", rel_path));
    impl->device->set_name(image_view.value(), std::format("{} Image View", rel_path));

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

    asset->acquire_ref();
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

auto AssetManager::unload_texture(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    if (!(asset->is_loaded() && asset->release_ref())) {
        return;
    }

    impl->textures.destroy_slot(asset->texture_id);
    asset->texture_id = TextureID::Invalid;
}

auto AssetManager::load_material(const UUID &uuid, const Material &material_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    asset->material_id = impl->materials.create_slot(const_cast<Material &&>(material_info));

    auto *albedo_texture = this->get_texture(material_info.albedo_texture);
    auto *normal_texture = this->get_texture(material_info.normal_texture);
    auto *emissive_texture = this->get_texture(material_info.emissive_texture);

    // TODO: Implement a checkerboard image for invalid textures
    u64 gpu_buffer_offset = SlotMap_decode_id(asset->material_id).index * sizeof(GPU::Material);
    GPU::Material gpu_material = {
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
    transfer_man.wait_on(transfer_man.upload_staging(
        ls::span(ls::bit_cast<u8 *>(&gpu_material), sizeof(GPU::Material)), impl->material_buffer, gpu_buffer_offset));

    asset->acquire_ref();
    return true;
}

auto AssetManager::unload_material(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    if (!(asset->is_loaded() && asset->release_ref())) {
        return;
    }

    auto *material = this->get_material(asset->material_id);
    if (material->albedo_texture) {
        this->unload_texture(material->albedo_texture);
    }

    if (material->normal_texture) {
        this->unload_texture(material->normal_texture);
    }

    if (material->emissive_texture) {
        this->unload_texture(material->emissive_texture);
    }

    impl->materials.destroy_slot(asset->material_id);
    asset->model_id = ModelID::Invalid;
}

auto AssetManager::load_scene(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    asset->scene_id = impl->scenes.create_slot();
    auto *scene = impl->scenes.slot(asset->scene_id);

    if (!scene->init("unnamed_scene")) {
        return false;
    }

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
                    ls::match{
                        [](const auto &) {},
                        [&](UUID *v) {
                            if (*v) {
                                cached_assets.emplace(*v);
                            }
                        },
                    },
                    member);
            });
        });
    });

    for (const auto &cached_uuid : cached_assets) {
        this->load_asset(cached_uuid);
    }

    asset->acquire_ref();
    return true;
}

auto AssetManager::unload_scene(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    if (!(asset->is_loaded() && asset->release_ref())) {
        return;
    }

    auto *scene = this->get_scene(asset->scene_id);
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
                    ls::match{
                        [](const auto &) {},
                        [&](UUID *v) {
                            if (*v) {
                                cached_assets.emplace(*v);
                            }
                        },
                    },
                    member);
            });
        });
    });

    for (const auto &cached_uuid : cached_assets) {
        this->unload_asset(cached_uuid);
    }

    scene->destroy();

    impl->scenes.destroy_slot(asset->scene_id);
    asset->scene_id = SceneID::Invalid;
}

auto AssetManager::export_asset(const UUID &uuid, const fs::path &path) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);

    JsonWriter json = {};
    this->begin_asset_meta(json, uuid, asset->type);

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

    return this->end_asset_meta(json, path);
}

auto AssetManager::export_texture(const UUID &uuid, JsonWriter &json, const fs::path &) -> bool {
    ZoneScoped;

    auto *texture = this->get_texture(uuid);
    LS_EXPECT(texture);
    return this->write_texture_asset_meta(json, texture);
}

auto AssetManager::export_model(const UUID &uuid, JsonWriter &json, const fs::path &) -> bool {
    ZoneScoped;

    auto *model = this->get_model(uuid);
    LS_EXPECT(model);
    return this->write_model_asset_meta(json, model);
}

auto AssetManager::export_scene(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool {
    ZoneScoped;

    auto *scene = this->get_scene(uuid);
    LS_EXPECT(scene);
    this->write_scene_asset_meta(json, scene);

    return scene->export_to_file(path);
}

auto AssetManager::delete_asset(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (asset->ref_count > 0) {
        LOG_WARN("Deleting alive asset {} with {} references!", asset->uuid.str(), asset->ref_count);
    }

    asset->ref_count = 0;
    this->unload_asset(uuid);
    impl->registry.erase(uuid);

    LOG_TRACE("Deleted asset {}.", uuid.str());
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

    return impl->models.slot(asset->model_id);
}

auto AssetManager::get_model(ModelID model_id) -> Model * {
    ZoneScoped;

    if (model_id == ModelID::Invalid) {
        return nullptr;
    }

    return impl->models.slot(model_id);
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

    return impl->textures.slot(asset->texture_id);
}

auto AssetManager::get_texture(TextureID texture_id) -> Texture * {
    ZoneScoped;

    if (texture_id == TextureID::Invalid) {
        return nullptr;
    }

    return impl->textures.slot(texture_id);
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

    return impl->materials.slot(asset->material_id);
}

auto AssetManager::get_material(MaterialID material_id) -> Material * {
    ZoneScoped;

    if (material_id == MaterialID::Invalid) {
        return nullptr;
    }

    return impl->materials.slot(material_id);
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

    return impl->scenes.slot(asset->scene_id);
}

auto AssetManager::get_scene(SceneID scene_id) -> Scene * {
    ZoneScoped;

    if (scene_id == SceneID::Invalid) {
        return nullptr;
    }

    return impl->scenes.slot(scene_id);
}

auto AssetManager::begin_asset_meta(JsonWriter &json, const UUID &uuid, AssetType type) -> void {
    ZoneScoped;

    json.begin_obj();
    json["uuid"] = uuid.str();
    json["type"] = std::to_underlying(type);
}

auto AssetManager::write_texture_asset_meta(JsonWriter &, Texture *) -> bool {
    ZoneScoped;

    return true;
}

auto AssetManager::write_model_asset_meta(JsonWriter &json, Model *model) -> bool {
    ZoneScoped;

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

auto AssetManager::write_scene_asset_meta(JsonWriter &json, Scene *scene) -> bool {
    ZoneScoped;

    json["name"] = scene->get_name_sv();

    return true;
}

auto AssetManager::end_asset_meta(JsonWriter &json, const fs::path &path) -> bool {
    ZoneScoped;

    json.end_obj();

    auto meta_path = path.string() + ".lrasset";
    File file(meta_path, FileAccess::Write);
    if (!file) {
        return false;
    }

    file.write(json.stream.view().data(), json.stream.view().length());
    file.close();

    return true;
}

}  // namespace lr
