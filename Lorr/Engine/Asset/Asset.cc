#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserKTX2.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Core/Logger.hh"
#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Memory/Hasher.hh"

#include "Engine/OS/File.hh"

#include "Engine/Scene/ECSModule/Core.hh"

#include <meshoptimizer.h>
#include <simdjson.h>

namespace lr {
template<glm::length_t N, typename T>
bool json_to_vec(simdjson::ondemand::value &o, glm::vec<N, T> &vec) {
    using U = glm::vec<N, T>;
    for (i32 i = 0; i < U::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        vec[i] = static_cast<T>(o[components[i]].get_double().value_unsafe());
    }

    return true;
}

auto begin_asset_meta(JsonWriter &json, const UUID &uuid, AssetType type) -> void {
    ZoneScoped;

    json.begin_obj();
    json["uuid"] = uuid.str();
    json["type"] = std::to_underlying(type);
}

auto write_texture_asset_meta(JsonWriter &, Texture *) -> bool {
    ZoneScoped;

    return true;
}

auto write_material_asset_meta(JsonWriter &json, UUID &uuid, Material &material) -> bool {
    ZoneScoped;

    json.begin_obj();
    json["uuid"] = uuid.str();
    json["albedo_color"] = material.albedo_color;
    json["emissive_color"] = material.emissive_color;
    json["roughness_factor"] = material.roughness_factor;
    json["metallic_factor"] = material.metallic_factor;
    json["alpha_mode"] = std::to_underlying(material.alpha_mode);
    json["alpha_cutoff"] = material.alpha_cutoff;
    json["albedo_texture"] = material.albedo_texture.str();
    json["normal_texture"] = material.normal_texture.str();
    json["emissive_texture"] = material.emissive_texture.str();
    json["metallic_roughness_texture"] = material.metallic_roughness_texture.str();
    json["occlusion_texture"] = material.occlusion_texture.str();
    json.end_obj();

    return true;
}

auto write_model_asset_meta(JsonWriter &json, ls::span<UUID> embedded_texture_uuids, ls::span<UUID> material_uuids, ls::span<Material> materials)
    -> bool {
    ZoneScoped;

    json["embedded_textures"].begin_array();
    for (const auto &uuid : embedded_texture_uuids) {
        json << uuid.str();
    }
    json.end_array();

    json["embedded_materials"].begin_array();
    for (const auto &[material_uuid, material] : std::views::zip(material_uuids, materials)) {
        write_material_asset_meta(json, material_uuid, material);
    }
    json.end_array();

    return true;
}

auto write_scene_asset_meta(JsonWriter &json, Scene *scene) -> bool {
    ZoneScoped;

    json["name"] = scene->get_name_sv();

    return true;
}

auto end_asset_meta(JsonWriter &json, const fs::path &path) -> bool {
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

template<>
struct Handle<AssetManager>::Impl {
    Device *device = nullptr;
    fs::path root_path = fs::current_path();
    AssetRegistry registry = {};

    std::shared_mutex registry_mutex = {};
    SlotMap<Model, ModelID> models = {};

    std::shared_mutex textures_mutex = {};
    SlotMap<Texture, TextureID> textures = {};

    std::shared_mutex materials_mutex = {};
    vuk::PersistentDescriptorSet materials_descriptor_set = {};
    SlotMap<Material, MaterialID> materials = {};
    std::vector<MaterialID> dirty_materials = {};
    Buffer materials_buffer = {};

    SlotMap<std::unique_ptr<Scene>, SceneID> scenes = {};
};

auto AssetManager::create(Device *device) -> AssetManager {
    ZoneScoped;

    auto impl = new Impl;
    auto self = AssetManager(impl);

    impl->device = device;
    impl->root_path = fs::current_path();

    BindlessDescriptorInfo bindless_set_info[] = {
        { .binding = 0, .type = vuk::DescriptorType::eSampler, .descriptor_count = 1024 },
        { .binding = 1, .type = vuk::DescriptorType::eSampledImage, .descriptor_count = 1024 },
    };
    impl->materials_descriptor_set = device->create_persistent_descriptor_set(bindless_set_info, 1).release();

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
        case AssetType::Directory:
            LS_DEBUGBREAK();
            return "";
    }

    LS_UNREACHABLE();
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
        case fnv64_c(".KTX2"):
            return AssetFileType::KTX2;
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
        case AssetType::Directory:
            LS_DEBUGBREAK();
            return "";
    }

    LS_UNREACHABLE();
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
    asset->scene_id = impl->scenes.create_slot(std::make_unique<Scene>());
    auto *scene = impl->scenes.slot(asset->scene_id)->get();
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

    if (!fs::exists(path)) {
        LOG_ERROR("Trying to import an asset '{}' that doesn't exist.", path);
        return UUID(nullptr);
    }

    auto asset_type = AssetType::None;
    switch (this->to_asset_file_type(path)) {
        case AssetFileType::Meta: {
            return this->register_asset(path);
        }
        case AssetFileType::GLB:
        case AssetFileType::GLTF: {
            asset_type = AssetType::Model;
            break;
        }
        case AssetFileType::PNG:
        case AssetFileType::JPEG:
        case AssetFileType::KTX2: {
            asset_type = AssetType::Texture;
            break;
        }
        default: {
            return UUID(nullptr);
        }
    }

    // Check for meta file before creating new asset
    auto meta_path = stack.format("{}.lrasset", path);
    if (fs::exists(meta_path)) {
        return this->register_asset(meta_path);
    }

    auto uuid = this->create_asset(asset_type, path);
    if (!uuid) {
        return UUID(nullptr);
    }

    JsonWriter json;
    begin_asset_meta(json, uuid, asset_type);

    switch (asset_type) {
        case AssetType::Model: {
            auto gltf_model = GLTFModelInfo::parse_info(path);
            auto textures = std::vector<UUID>();
            auto embedded_textures = std::vector<UUID>();
            for (auto &v : gltf_model->textures) {
                auto &image = gltf_model->images[v.image_index.value()];
                auto &texture_uuid = textures.emplace_back();
                std::visit(
                    ls::match{
                        [&](const std::vector<u8> &) { //
                            texture_uuid = this->create_asset(AssetType::Texture, path);
                            embedded_textures.push_back(texture_uuid);
                        },
                        [&](const fs::path &image_path) { //
                            texture_uuid = this->import_asset(image_path);
                        },
                    },
                    image.image_data
                );
            }

            auto material_uuids = std::vector<UUID>(gltf_model->materials.size());
            auto materials = std::vector<Material>(gltf_model->materials.size());
            for (const auto &[material_uuid, material, gltf_material] : std::views::zip(material_uuids, materials, gltf_model->materials)) {
                material_uuid = this->create_asset(AssetType::Material);
                material.albedo_color = gltf_material.albedo_color;
                material.emissive_color = gltf_material.emissive_color;
                material.roughness_factor = gltf_material.roughness_factor;
                material.metallic_factor = gltf_material.metallic_factor;
                material.alpha_mode = static_cast<AlphaMode>(gltf_material.alpha_mode);
                material.alpha_cutoff = gltf_material.alpha_cutoff;

                if (auto tex_idx = gltf_material.albedo_texture_index; tex_idx.has_value()) {
                    material.albedo_texture = textures[tex_idx.value()];
                }

                if (auto tex_idx = gltf_material.normal_texture_index; tex_idx.has_value()) {
                    material.normal_texture = textures[tex_idx.value()];
                }

                if (auto tex_idx = gltf_material.emissive_texture_index; tex_idx.has_value()) {
                    material.emissive_texture = textures[tex_idx.value()];
                }

                if (auto tex_idx = gltf_material.metallic_roughness_texture_index; tex_idx.has_value()) {
                    material.metallic_roughness_texture = textures[tex_idx.value()];
                }

                if (auto tex_idx = gltf_material.occlusion_texture_index; tex_idx.has_value()) {
                    material.occlusion_texture = textures[tex_idx.value()];
                }
            }

            write_model_asset_meta(json, embedded_textures, material_uuids, materials);
        } break;
        case AssetType::Texture: {
            Texture texture = {};

            write_texture_asset_meta(json, &texture);
        } break;
        default:;
    }

    if (!end_asset_meta(json, path)) {
        return UUID(nullptr);
    }

    return uuid;
}

struct AssetMetaFile {
    simdjson::padded_string contents;
    simdjson::ondemand::parser parser;
    simdjson::simdjson_result<simdjson::ondemand::document> doc;
};

auto read_meta_file(const fs::path &path) -> std::unique_ptr<AssetMetaFile> {
    ZoneScoped;

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
    auto uuid = UUID::from_string(uuid_json.value_unsafe()).value();
    auto type = static_cast<AssetType>(type_json.value_unsafe().get_uint64());

    if (!this->register_asset(uuid, type, asset_path)) {
        return UUID(nullptr);
    }

    return uuid;
}

auto AssetManager::register_asset(const UUID &uuid, AssetType type, const fs::path &path) -> bool {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(uuid);
    if (!inserted) {
        if (asset_it != impl->registry.end()) {
            // Tried a reinsert, asset already exists
            return true;
        }

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
    LS_EXPECT(asset);
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
        // Acquire all child assets
        auto *model = impl->models.slot(asset->model_id);
        asset->acquire_ref();

        for (const auto &v : model->materials) {
            this->load_material(v, {});
        }

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

    auto asset_path = asset->path;
    asset->acquire_ref();

    // Below we register new assets, which causes asset pointer to be invalidated.
    // set this to nullptr so it's obvious when debugging.
    asset = nullptr;

    auto embedded_textures = std::vector<UUID>();
    auto embedded_texture_uuids_json = meta_json->doc["embedded_textures"].get_array();
    for (auto embedded_texture_uuid_json : embedded_texture_uuids_json) {
        auto embedded_texture_uuid_str = embedded_texture_uuid_json.get_string();
        if (embedded_texture_uuid_str.error()) {
            LOG_ERROR("Failed to import model {}! An embedded texture with corrupt UUID.", asset_path);
            return false;
        }

        auto embedded_texture_uuid = UUID::from_string(embedded_texture_uuid_str.value_unsafe());
        if (!embedded_texture_uuid.has_value()) {
            LOG_ERROR("Failed to import model {}! An embedded texture with corrupt UUID.", asset_path);
            return false;
        }

        embedded_textures.push_back(embedded_texture_uuid.value());
        this->register_asset(embedded_texture_uuid.value(), AssetType::Texture, asset_path);
    }

    // Load registered UUIDs.
    auto materials_json = meta_json->doc["embedded_materials"].get_array();
    if (materials_json.error()) {
        LOG_ERROR("Failed to import model {}! Missing materials filed.", asset_path);
        return false;
    }

    auto materials = std::vector<Material>();
    for (auto material_json : materials_json) {
        if (material_json.error()) {
            LOG_ERROR("Failed to import model {}! A material with error.", asset_path);
            return false;
        }

        if (auto material_uuid_json = material_json["uuid"]; !material_uuid_json.error()) {
            auto material_uuid = UUID::from_string(material_uuid_json.value_unsafe());
            if (!material_uuid.has_value()) {
                LOG_ERROR("Failed to import model {}! A material with corrupt UUID.", asset_path);
                return false;
            }

            this->register_asset(material_uuid.value(), AssetType::Material, asset_path);
            model->materials.emplace_back(material_uuid.value());
        }

        auto &material = materials.emplace_back();
        if (auto member_json = material_json["albedo_color"]; !member_json.error()) {
            json_to_vec(member_json.value_unsafe(), material.albedo_color);
        }
        if (auto member_json = material_json["emissive_color"]; !member_json.error()) {
            json_to_vec(member_json.value_unsafe(), material.emissive_color);
        }
        if (auto member_json = material_json["roughness_factor"]; !member_json.error()) {
            material.roughness_factor = static_cast<f32>(member_json.get_double().value_unsafe());
        }
        if (auto member_json = material_json["metallic_factor"]; !member_json.error()) {
            material.metallic_factor = static_cast<f32>(member_json.get_double().value_unsafe());
        }
        if (auto member_json = material_json["alpha_mode"]; !member_json.error()) {
            material.alpha_mode = static_cast<AlphaMode>(member_json.get_uint64().value_unsafe());
        }
        if (auto member_json = material_json["alpha_cutoff"]; !member_json.error()) {
            material.alpha_cutoff = static_cast<f32>(member_json.get_double().value_unsafe());
        }
        if (auto member_json = material_json["albedo_texture"]; !member_json.error()) {
            material.albedo_texture = UUID::from_string(member_json.get_string().value_unsafe()).value_or(UUID(nullptr));
        }
        if (auto member_json = material_json["normal_texture"]; !member_json.error()) {
            material.normal_texture = UUID::from_string(member_json.get_string().value_unsafe()).value_or(UUID(nullptr));
        }
        if (auto member_json = material_json["emissive_texture"]; !member_json.error()) {
            material.emissive_texture = UUID::from_string(member_json.get_string().value_unsafe()).value_or(UUID(nullptr));
        }
        if (auto member_json = material_json["metallic_roughness_texture"]; !member_json.error()) {
            material.metallic_roughness_texture = UUID::from_string(member_json.get_string().value_unsafe()).value_or(UUID(nullptr));
        }
        if (auto member_json = material_json["occlusion_texture"]; !member_json.error()) {
            material.occlusion_texture = UUID::from_string(member_json.get_string().value_unsafe()).value_or(UUID(nullptr));
        }
    }

    for (const auto &[material_uuid, material] : std::views::zip(model->materials, materials)) {
        this->load_material(material_uuid, material);
    }

    struct GLTFCallbacks {
        Model *model = nullptr;

        std::vector<glm::vec3> vertex_positions = {};
        std::vector<glm::vec3> vertex_normals = {};
        std::vector<glm::vec2> vertex_texcoords = {};
        std::vector<Model::Index> indices = {};
    };
    auto on_new_primitive =
        [](void *user_data, u32 mesh_index, u32 material_index, u32 vertex_offset, u32 vertex_count, u32 index_offset, u32 index_count) {
            auto &app = Application::get();
            auto *info = static_cast<GLTFCallbacks *>(user_data);
            if (info->model->meshes.size() <= mesh_index) {
                info->model->meshes.resize(mesh_index + 1);
            }

            auto &mesh = info->model->meshes[mesh_index];
            auto primitive_index = info->model->primitives.size();
            auto &primitive = info->model->primitives.emplace_back();
            auto *material_asset = app.asset_man.get_asset(info->model->materials[material_index]);
            auto global_material_index = SlotMap_decode_id(material_asset->material_id).index;

            info->vertex_positions.resize(info->vertex_positions.size() + vertex_count);
            info->vertex_normals.resize(info->vertex_normals.size() + vertex_count);
            info->vertex_texcoords.resize(info->vertex_texcoords.size() + vertex_count);
            info->indices.resize(info->indices.size() + index_count);

            mesh.primitive_indices.push_back(primitive_index);
            primitive.material_index = global_material_index;
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
        asset_path,
        { .user_data = &gltf_callbacks,
          .on_new_primitive = on_new_primitive,
          .on_access_index = on_access_index,
          .on_access_position = on_access_position,
          .on_access_normal = on_access_normal,
          .on_access_texcoord = on_access_texcoord }
    );
    if (!gltf_model.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", asset_path);
        return false;
    }

    //  ── SCENE HIERARCHY ─────────────────────────────────────────────────
    for (const auto &node : gltf_model->nodes) {
        model->nodes.push_back(
            { .name = node.name,
              .child_indices = node.children,
              .mesh_index = node.mesh_index,
              .translation = node.translation,
              .rotation = node.rotation,
              .scale = node.scale }
        );
    }

    model->default_scene_index = gltf_model->defualt_scene_index.value_or(0_sz);
    for (const auto &scene : gltf_model->scenes) {
        model->scenes.push_back({ .name = scene.name, .node_indices = scene.node_indices });
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

            auto &primitive = model->primitives[primitive_index];
            auto vertex_offset = model_vertex_positions.size();
            auto index_offset = model_indices.size();
            auto triangle_offset = model_local_triangle_indices.size();
            auto meshlet_offset = model_meshlets.size();

            auto raw_indices = ls::span(gltf_callbacks.indices.data() + primitive.index_offset, primitive.index_count);
            auto raw_vertex_positions = ls::span(gltf_callbacks.vertex_positions.data() + primitive.vertex_offset, primitive.vertex_count);
            auto raw_vertex_normals = ls::span(gltf_callbacks.vertex_normals.data() + primitive.vertex_offset, primitive.vertex_count);

            auto meshlets = std::vector<GPU::Meshlet>();
            auto meshlet_bounds = std::vector<GPU::MeshletBounds>();
            auto meshlet_indices = std::vector<u32>();
            auto local_triangle_indices = std::vector<u8>();
            {
                ZoneScopedN("Build Meshlets");
                // Worst case count
                auto max_meshlets = meshopt_buildMeshletsBound( //
                    raw_indices.size(),
                    Model::MAX_MESHLET_INDICES,
                    Model::MAX_MESHLET_PRIMITIVES
                );
                auto raw_meshlets = std::vector<meshopt_Meshlet>(max_meshlets);
                meshlet_indices.resize(max_meshlets * Model::MAX_MESHLET_INDICES);
                local_triangle_indices.resize(max_meshlets * Model::MAX_MESHLET_PRIMITIVES * 3);
                auto meshlet_count = meshopt_buildMeshlets( //
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
                    0.0
                );

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

                primitive.meshlet_count = meshlet_count;
                primitive.meshlet_offset = meshlet_offset;
                primitive.local_triangle_indices_offset = triangle_offset;
            }

            std::ranges::move(raw_vertex_positions, std::back_inserter(model_vertex_positions));
            std::ranges::move(meshlet_indices, std::back_inserter(model_indices));
            std::ranges::move(meshlets, std::back_inserter(model_meshlets));
            std::ranges::move(meshlet_bounds, std::back_inserter(model_meshlet_bounds));
            std::ranges::move(local_triangle_indices, std::back_inserter(model_local_triangle_indices));
        }
    }

    auto &transfer_man = impl->device->transfer_man();
    model->indices = Buffer::create(*impl->device, ls::size_bytes(model_indices)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_indices), model->indices));

    model->vertex_positions = Buffer::create(*impl->device, ls::size_bytes(model_vertex_positions)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_vertex_positions), model->vertex_positions));

    model->vertex_normals = Buffer::create(*impl->device, ls::size_bytes(gltf_callbacks.vertex_normals)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(gltf_callbacks.vertex_normals), model->vertex_normals));

    if (!gltf_callbacks.vertex_texcoords.empty()) {
        model->texture_coords = Buffer::create(*impl->device, ls::size_bytes(gltf_callbacks.vertex_texcoords)).value();
        transfer_man.wait_on(transfer_man.upload_staging(ls::span(gltf_callbacks.vertex_texcoords), model->texture_coords));
    }

    model->meshlets = Buffer::create(*impl->device, ls::size_bytes(model_meshlets)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_meshlets), model->meshlets));

    model->meshlet_bounds = Buffer::create(*impl->device, ls::size_bytes(model_meshlet_bounds)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_meshlet_bounds), model->meshlet_bounds));

    model->local_triangle_indices = Buffer::create(*impl->device, ls::size_bytes(model_local_triangle_indices)).value();
    transfer_man.wait_on(transfer_man.upload_staging(ls::span(model_local_triangle_indices), model->local_triangle_indices));

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
    for (auto &v : model->materials) {
        this->unload_material(v);
    }

    model->materials.clear();
    model->primitives.clear();
    model->meshes.clear();
    model->nodes.clear();
    impl->device->destroy(model->indices.id());
    impl->device->destroy(model->vertex_positions.id());
    impl->device->destroy(model->vertex_normals.id());
    if (model->texture_coords) {
        impl->device->destroy(model->texture_coords.id());
    }
    impl->device->destroy(model->meshlets.id());
    impl->device->destroy(model->meshlet_bounds.id());
    impl->device->destroy(model->local_triangle_indices.id());

    impl->models.destroy_slot(asset->model_id);
    asset->model_id = ModelID::Invalid;
}

auto AssetManager::load_texture(const UUID &uuid, const TextureInfo &info) -> bool {
    ZoneScoped;
    memory::ScopedStack stack;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    asset->acquire_ref();

    if (asset->is_loaded()) {
        return true;
    }

    auto file_type = info.file_type;
    auto raw_data = std::vector<u8>(info.pixels.begin(), info.pixels.end());
    if (info.pixels.empty()) {
        if (!asset->path.has_extension()) {
            LOG_ERROR("Trying to load texture \"{}\" without a file extension.", asset->path);
            return false;
        }

        raw_data = File::to_bytes(asset->path);
        if (raw_data.empty()) {
            LOG_ERROR("Error reading '{}'. Invalid texture file? Notice the question mark.", asset->path);
            return false;
        }

        file_type = this->to_asset_file_type(asset->path);
    }

    auto format = vuk::Format::eUndefined;
    auto extent = vuk::Extent3D{};
    auto mip_level_count = 1_u32;
    switch (file_type) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            auto image_info = STBImageInfo::parse_info(raw_data);
            if (!image_info.has_value()) {
                return false;
            }
            extent = image_info->extent;
            format = info.use_srgb ? vuk::Format::eR8G8B8A8Srgb : vuk::Format::eR8G8B8A8Unorm;
            mip_level_count = static_cast<u32>(glm::floor(glm::log2(static_cast<f32>(ls::max(extent.width, extent.height)))) + 1);
        } break;
        case AssetFileType::KTX2: {
            auto image_info = KTX2ImageInfo::parse_info(raw_data);
            if (!image_info.has_value()) {
                return false;
            }
            extent = image_info->base_extent;
            format = info.use_srgb ? vuk::Format::eBc7SrgbBlock : vuk::Format::eBc7UnormBlock;
            mip_level_count = image_info->mip_level_count;
        } break;
        default: {
            LOG_ERROR("Failed to load texture '{}', invalid extension.", asset->path);
            return false;
        }
    }

    auto rel_path = fs::relative(asset->path, impl->root_path);
    auto image_info = ImageInfo{
        .format = format,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferSrc,
        .type = vuk::ImageType::e2D,
        .extent = extent,
        .slice_count = 1,
        .mip_count = mip_level_count,
        .name = stack.format("{} Image", rel_path),
    };
    auto [image, image_view] = Image::create_with_view(*impl->device, image_info).value();
    auto dst_attachment = image_view.discard(*impl->device, "dst image", vuk::ImageUsageFlagBits::eTransferDst);
    auto alignment = vuk::format_to_texel_block_size(format);

    auto sampler_info = SamplerInfo{
        .min_filter = vuk::Filter::eLinear,
        .mag_filter = vuk::Filter::eLinear,
        .mipmap_mode = vuk::SamplerMipmapMode::eLinear,
        .addr_u = vuk::SamplerAddressMode::eRepeat,
        .addr_v = vuk::SamplerAddressMode::eRepeat,
        .addr_w = vuk::SamplerAddressMode::eRepeat,
        .compare_op = vuk::CompareOp::eNever,
        .max_anisotropy = 8.0f,
        .mip_lod_bias = 0.0f,
        .min_lod = 0.0f,
        .max_lod = static_cast<f32>(mip_level_count - 1),
        .use_anisotropy = true,
    };
    auto sampler = Sampler::create(*impl->device, sampler_info).value();

    auto &transfer_man = impl->device->transfer_man();
    switch (file_type) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            ZoneScopedN("Parse STB");
            auto parsed_image = STBImageInfo::parse(raw_data);
            if (!parsed_image.has_value()) {
                return false;
            }

            auto image_data = std::move(parsed_image->data);
            auto buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, ls::size_bytes(image_data), alignment);
            std::memcpy(buffer->mapped_ptr, image_data.data(), image_data.size());

            dst_attachment = vuk::copy(std::move(buffer), std::move(dst_attachment));
            dst_attachment = vuk::generate_mips(std::move(dst_attachment), 0, mip_level_count - 1);
            dst_attachment.as_released(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
            transfer_man.wait_on(std::move(dst_attachment));
        } break;
        case AssetFileType::KTX2: {
            ZoneScopedN("Parse KTX");
            auto parsed_image = KTX2ImageInfo::parse(raw_data);
            if (!parsed_image.has_value()) {
                return false;
            }
            auto image_data = std::move(parsed_image->data);

            for (u32 level = 0; level < parsed_image->mip_level_count; level++) {
                ZoneScoped;
                ZoneTextF("Upload KTX mip %u", level);
                auto mip_data_offset = parsed_image->per_level_offsets[level];
                auto level_extent = vuk::Extent3D{
                    .width = parsed_image->base_extent.width >> level,
                    .height = parsed_image->base_extent.height >> level,
                    .depth = 1,
                };
                auto size = vuk::compute_image_size(format, level_extent);
                auto buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, size, alignment);

                // TODO, WARN: size param might not be safe. Check with asan.
                std::memcpy(buffer->mapped_ptr, image_data.data() + mip_data_offset, size);
                auto dst_mip = dst_attachment.mip(level);
                vuk::copy(std::move(buffer), std::move(dst_mip));
            }

            dst_attachment = dst_attachment.as_released(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
            transfer_man.wait_on(std::move(dst_attachment));
        } break;
        default: {
            LOG_ERROR("Failed to load texture '{}', invalid extension.", asset->path);
            return false;
        }
    }

    impl->textures_mutex.lock();
    asset->texture_id = impl->textures.create_slot(Texture{ .image = image, .image_view = image_view, .sampler = sampler });
    impl->textures_mutex.unlock();

    LOG_TRACE("Loaded texture {} {}.", asset->uuid.str(), SlotMap_decode_id(asset->texture_id).index);

    return true;
}

auto AssetManager::unload_texture(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (!asset || (!(asset->is_loaded() && asset->release_ref()))) {
        return;
    }

    auto *texture = this->get_texture(asset->texture_id);
    impl->device->destroy(texture->image_view.id());
    impl->device->destroy(texture->image.id());

    LOG_TRACE("Unloaded texture {}.", uuid.str());

    impl->textures.destroy_slot(asset->texture_id);
    asset->texture_id = TextureID::Invalid;
}

auto AssetManager::is_texture_loaded(const UUID &uuid) -> bool {
    ZoneScoped;

    std::shared_lock _(impl->textures_mutex);
    auto *asset = this->get_asset(uuid);
    if (!asset) {
        return false;
    }

    return asset->is_loaded();
}

auto AssetManager::load_material(const UUID &uuid, const Material &material_info) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    LS_EXPECT(asset);
    // Materials don't explicitly load any resources, they need to increase child resources refs.
    // if (asset->is_loaded()) {
    //     asset->acquire_ref();
    //     return true;
    // }

    if (!asset->is_loaded()) {
        asset->material_id = impl->materials.create_slot(const_cast<Material &&>(material_info));
    }

    auto *material = impl->materials.slot(asset->material_id);
    auto &app = Application::get();

    this->set_material_dirty(asset->material_id);

    if (material->albedo_texture) {
        auto job = Job::create([this, texture_uuid = material->albedo_texture, material_id = asset->material_id]() {
            this->load_texture(texture_uuid, {});
            this->set_material_dirty(material_id);
        });
        app.job_man->submit(std::move(job));
    }

    if (material->normal_texture) {
        auto job = Job::create([this, texture_uuid = material->normal_texture, material_id = asset->material_id]() {
            this->load_texture(texture_uuid, { .use_srgb = false });
            this->set_material_dirty(material_id);
        });
        app.job_man->submit(std::move(job));
    }

    if (material->emissive_texture) {
        auto job = Job::create([this, texture_uuid = material->emissive_texture, material_id = asset->material_id]() {
            this->load_texture(texture_uuid, {});
            this->set_material_dirty(material_id);
        });
        app.job_man->submit(std::move(job));
    }

    if (material->metallic_roughness_texture) {
        auto job = Job::create([this, texture_uuid = material->metallic_roughness_texture, material_id = asset->material_id]() {
            this->load_texture(texture_uuid, { .use_srgb = false });
            this->set_material_dirty(material_id);
        });
        app.job_man->submit(std::move(job));
    }

    if (material->occlusion_texture) {
        auto job = Job::create([this, texture_uuid = material->occlusion_texture, material_id = asset->material_id]() {
            this->load_texture(texture_uuid, { .use_srgb = false });
            this->set_material_dirty(material_id);
        });
        app.job_man->submit(std::move(job));
    }

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

    if (material->metallic_roughness_texture) {
        this->unload_texture(material->metallic_roughness_texture);
    }

    if (material->occlusion_texture) {
        this->unload_texture(material->occlusion_texture);
    }

    impl->materials.destroy_slot(asset->material_id);
    asset->material_id = MaterialID::Invalid;
}

auto AssetManager::is_material_loaded(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    // Parent asset is not loaded, skip
    if (!asset || !asset->is_loaded()) {
        return false;
    }

    auto *material = this->get_material(asset->material_id);
    if (material->albedo_texture && this->is_texture_loaded(material->albedo_texture)) {
        return false;
    }

    if (material->normal_texture && this->is_texture_loaded(material->normal_texture)) {
        return false;
    }

    if (material->emissive_texture && this->is_texture_loaded(material->emissive_texture)) {
        return false;
    }

    if (material->metallic_roughness_texture && this->is_texture_loaded(material->metallic_roughness_texture)) {
        return false;
    }

    if (material->occlusion_texture && this->is_texture_loaded(material->occlusion_texture)) {
        return false;
    }

    return true;
}

auto AssetManager::load_scene(const UUID &uuid) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    asset->scene_id = impl->scenes.create_slot(std::make_unique<Scene>());
    auto *scene = impl->scenes.slot(asset->scene_id)->get();

    if (!scene->init("unnamed_scene")) {
        return false;
    }

    if (!scene->import_from_file(asset->path)) {
        return false;
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
    scene->destroy();

    impl->scenes.destroy_slot(asset->scene_id);
    asset->scene_id = SceneID::Invalid;
}

auto AssetManager::export_asset(const UUID &uuid, const fs::path &path) -> bool {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);

    JsonWriter json = {};
    begin_asset_meta(json, uuid, asset->type);

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

    return end_asset_meta(json, path);
}

auto AssetManager::export_texture(const UUID &uuid, JsonWriter &json, const fs::path &) -> bool {
    ZoneScoped;

    auto *texture = this->get_texture(uuid);
    LS_EXPECT(texture);
    return write_texture_asset_meta(json, texture);
}

auto AssetManager::export_model(const UUID &uuid, JsonWriter &json, const fs::path &) -> bool {
    ZoneScoped;

    auto *model = this->get_model(uuid);
    LS_EXPECT(model);

    auto materials = std::vector<Material>(model->materials.size());
    for (const auto &[material_uuid, material] : std::views::zip(model->materials, materials)) {
        material = *this->get_material(material_uuid);
    }

    return write_model_asset_meta(json, model->embedded_textures, model->materials, materials);
}

auto AssetManager::export_scene(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool {
    ZoneScoped;

    auto *scene = this->get_scene(uuid);
    LS_EXPECT(scene);
    write_scene_asset_meta(json, scene);

    return scene->export_to_file(path);
}

auto AssetManager::delete_asset(const UUID &uuid) -> void {
    ZoneScoped;

    auto *asset = this->get_asset(uuid);
    if (asset->ref_count > 0) {
        LOG_WARN("Deleting alive asset {} with {} references!", asset->uuid.str(), asset->ref_count);
    }

    if (asset->is_loaded()) {
        asset->ref_count = ls::min(asset->ref_count, 1_u64);
        this->unload_asset(uuid);
        impl->registry.erase(uuid);
    }

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

    return impl->scenes.slot(asset->scene_id)->get();
}

auto AssetManager::get_scene(SceneID scene_id) -> Scene * {
    ZoneScoped;

    if (scene_id == SceneID::Invalid) {
        return nullptr;
    }

    return impl->scenes.slot(scene_id)->get();
}

auto AssetManager::set_material_dirty(MaterialID material_id) -> void {
    ZoneScoped;

    std::shared_lock shared_lock(impl->materials_mutex);
    if (std::ranges::find(impl->dirty_materials, material_id) != impl->dirty_materials.end()) {
        return;
    }

    shared_lock.unlock();
    impl->materials_mutex.lock();
    impl->dirty_materials.emplace_back(material_id);
    impl->materials_mutex.unlock();
}

auto AssetManager::get_materials_buffer() -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto uuid_to_index = [this](UUID &uuid) -> u32 {
        if (!this->is_texture_loaded(uuid)) {
            return ~0_u32;
        }

        auto *texture_asset = this->get_asset(uuid);
        auto *texture = this->get_texture(texture_asset->texture_id);
        auto texture_index = SlotMap_decode_id(texture_asset->texture_id).index;
        auto *image_view = impl->device->image_view(texture->image_view.id());
        auto *sampler = impl->device->sampler(texture->sampler.id());

        impl->materials_descriptor_set.update_sampler(0, texture_index, *sampler);
        impl->materials_descriptor_set.update_sampled_image(1, texture_index, *image_view, vuk::ImageLayout::eShaderReadOnlyOptimal);

        return texture_index;
    };

    auto to_gpu_material = [&](Material *material) -> GPU::Material {
        return {
            .albedo_color = material->albedo_color,
            .emissive_color = material->emissive_color,
            .roughness_factor = material->roughness_factor,
            .metallic_factor = material->metallic_factor,
            .alpha_mode = static_cast<GPU::AlphaMode>(material->alpha_mode),
            .alpha_cutoff = material->alpha_cutoff,
            .albedo_image_index = uuid_to_index(material->albedo_texture),
            .normal_image_index = uuid_to_index(material->normal_texture),
            .emissive_image_index = uuid_to_index(material->emissive_texture),
            .metallic_roughness_image_index = uuid_to_index(material->metallic_roughness_texture),
            .occlusion_image_index = uuid_to_index(material->occlusion_texture),
        };
    };

    std::shared_lock shared_lock(impl->materials_mutex);
    if (impl->materials.size() == 0) {
        return {};
    }

    auto materials_buffer = vuk::Value<vuk::Buffer>{};
    bool rebuild_materials = false;
    auto gpu_materials_bytes_size = impl->materials.size() * sizeof(GPU::Material);
    if (gpu_materials_bytes_size > impl->materials_buffer.data_size()) {
        if (impl->materials_buffer.id() != BufferID::Invalid) {
            impl->device->wait();
            impl->device->destroy(impl->materials_buffer.id());
        }

        impl->materials_buffer = Buffer::create(*impl->device, gpu_materials_bytes_size, vuk::MemoryUsage::eGPUonly).value();
        materials_buffer = impl->materials_buffer.acquire(*impl->device, "materials buffer", vuk::eNone);
        vuk::fill(materials_buffer, ~0_u32);
        rebuild_materials = true;
    }

    LS_DEFER(&) {
        impl->dirty_materials.clear();
        impl->device->commit_descriptor_set(impl->materials_descriptor_set);
    };

    auto &transfer_man = impl->device->transfer_man();
    if (rebuild_materials) {
        auto dirty_materials_size_bytes = impl->materials.size() * sizeof(GPU::Material);
        auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, dirty_materials_size_bytes);
        auto *dst_material_ptr = reinterpret_cast<GPU::Material *>(upload_buffer->mapped_ptr);

        // All loaded materials
        auto all_materials = impl->materials.slots_unsafe();
        for (auto &dirty_material : all_materials) {
            auto gpu_material = to_gpu_material(&dirty_material);
            std::memcpy(dst_material_ptr, &gpu_material, sizeof(GPU::Material));
            dst_material_ptr++;
        }

        return transfer_man.upload_staging(std::move(upload_buffer), std::move(materials_buffer));
    }

    auto dirty_material_count = impl->dirty_materials.size();
    auto dirty_materials_size_bytes = impl->dirty_materials.size() * sizeof(GPU::Material);

    materials_buffer = impl->materials_buffer.acquire(*impl->device, "materials buffer", vuk::eNone);
    if (dirty_material_count == 0) {
        return materials_buffer;
    }

    auto upload_offsets = std::vector<u64>(dirty_material_count);
    auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, dirty_materials_size_bytes);
    auto *dst_material_ptr = reinterpret_cast<GPU::Material *>(upload_buffer->mapped_ptr);
    for (const auto &[dirty_material_id, offset] : std::views::zip(impl->dirty_materials, upload_offsets)) {
        auto index = SlotMap_decode_id(dirty_material_id).index;
        auto *material = this->get_material(dirty_material_id);
        auto gpu_material = to_gpu_material(material);

        std::memcpy(dst_material_ptr, &gpu_material, sizeof(GPU::Material));
        offset = index * sizeof(GPU::Material);
        dst_material_ptr++;
    }

    auto update_materials_pass = vuk::make_pass(
        "update materials",
        [upload_offsets = std::move(
             upload_offsets
         )](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_buffer, VUK_BA(vuk::Access::eTransferWrite) dst_buffer) {
            for (usize i = 0; i < upload_offsets.size(); i++) {
                auto offset = upload_offsets[i];
                auto src_subrange = src_buffer->subrange(i * sizeof(GPU::Material), sizeof(GPU::Material));
                auto dst_subrange = dst_buffer->subrange(offset, sizeof(GPU::Material));
                cmd_list.copy_buffer(src_subrange, dst_subrange);
            }

            return dst_buffer;
        }
    );

    return update_materials_pass(std::move(upload_buffer), std::move(materials_buffer));
}

auto AssetManager::get_materials_descriptor_set() -> vuk::PersistentDescriptorSet * {
    ZoneScoped;

    return &impl->materials_descriptor_set;
}

} // namespace lr
