#include "Engine/Asset/Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"
#include "Engine/Asset/ParserSTB.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Memory/Hasher.hh"
#include "Engine/Memory/PagedPool.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"
#include "Engine/World/Components.hh"

#include <simdjson.h>

namespace lr {
template<glm::length_t N, typename T>
bool json_to_vec(simdjson::ondemand::value &o, glm::vec<N, T> &vec) {
    using U = glm::vec<N, T>;
    for (i32 i = 0; i < U::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        vec[i] = static_cast<T>(o[components[i]].get_double());
    }

    return true;
}

template<>
struct Handle<AssetManager>::Impl {
    Device device = {};
    fs::path root_path = fs::current_path();
    AssetRegistry registry = {};

    PagedPool<Model, ModelID> models = {};
    PagedPool<Texture, TextureID> textures = {};
    PagedPool<Material, MaterialID, 1024_sz> materials = {};
    PagedPool<Scene::Impl *, SceneID, 128_sz> scenes = {};

    Buffer material_buffer = {};
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

auto AssetManager::register_asset(const Identifier &ident, const fs::path &path, AssetType type) -> Asset * {
    ZoneScoped;

    switch (type) {
        case AssetType::Model: {
            return this->register_model(ident, path);
        }
        case AssetType::Texture: {
            return this->register_texture(ident, path);
        }
        case AssetType::Material: {
            return this->register_material(ident, path);
        }
        case AssetType::Scene: {
            return this->register_scene(ident, path);
        }
        case AssetType::None:
        case AssetType::Shader:
        case AssetType::Font:
            LS_EXPECT(false);  // TODO: Other asset types
            return nullptr;
    }
}

auto AssetManager::register_asset_from_file(const fs::path &path) -> Asset * {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

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

    auto guid_json = doc["guid"].get_string();
    if (guid_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `guid` is missing.");
        return nullptr;
    }

    auto type_json = doc["type"].get_number();
    if (type_json.error()) {
        LOG_ERROR("Failed to read asset meta file. `type` is missing.");
        return nullptr;
    }

    auto ident = Identifier(guid_json.value());
    auto asset_path = path;
    asset_path.replace_extension("");
    auto type = static_cast<AssetType>(type_json.value().get_uint64());

    return this->register_asset(ident, asset_path, type);
}

auto AssetManager::register_model(const Identifier &ident, const fs::path &path) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(ident);
    if (!inserted) {
        LOG_ERROR("Cannot register new model!");
        return {};
    }
    auto &asset = asset_it->second;

    auto model_result = impl->models.create();
    if (!model_result.has_value()) {
        LOG_ERROR("Failed to create new model, out of pool space.");
        return {};
    }

    asset.path = path;
    asset.type = AssetType::Model;
    asset.model_id = model_result->id;

    return &asset;
}

auto AssetManager::register_texture(const Identifier &ident, const fs::path &path) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(ident);
    if (!inserted) {
        LOG_ERROR("Cannot register new texture!");
        return {};
    }
    auto &asset = asset_it->second;

    auto texture_result = impl->textures.create();
    if (!texture_result.has_value()) {
        LOG_ERROR("Failed to create new texture, out of pool space.");
        return {};
    }

    asset.path = path;
    asset.type = AssetType::Texture;
    asset.texture_id = texture_result->id;

    return &asset;
}

auto AssetManager::register_material(const Identifier &ident, const fs::path &path) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(ident);
    if (!inserted) {
        LOG_ERROR("Cannot register new scene!");
        return {};
    }
    auto &asset = asset_it->second;

    auto material_result = impl->materials.create();
    if (!material_result.has_value()) {
        LOG_ERROR("Failed to create new material, out of pool space.");
        return {};
    }

    asset.path = path;
    asset.type = AssetType::Material;
    asset.material_id = material_result->id;

    return &asset;
}

auto AssetManager::register_scene(const Identifier &ident, const fs::path &path) -> Asset * {
    ZoneScoped;

    auto [asset_it, inserted] = impl->registry.try_emplace(ident);
    if (!inserted) {
        LOG_ERROR("Cannot register new scene!");
        return {};
    }
    auto &asset = asset_it->second;

    auto scene_result = impl->scenes.create();
    if (!scene_result.has_value()) {
        LOG_ERROR("Failed to create new scene, out of pool space.");
        return {};
    }

    asset.path = path;
    asset.type = AssetType::Scene;
    asset.scene_id = scene_result->id;

    return &asset;
}

auto AssetManager::create_scene(Asset *asset, const std::string &name) -> Scene {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;

    LS_EXPECT(asset->type == AssetType::Scene);
    auto scene_result = Scene::create(name, &world);
    if (!scene_result.has_value()) {
        return Scene(nullptr);
    }

    auto &scene_impl = impl->scenes.get(asset->scene_id);
    scene_impl = scene_result->impl;

    return Scene(scene_impl);
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

    auto file_contents = File::to_bytes(path);
    if (file_contents.empty()) {
        LOG_ERROR("Cannot read texture file.");
        return TextureID::Invalid;
    }

    vk::Format format = {};
    vk::Extent3D extent = {};
    std::vector<u8> pixels = {};

    switch (this->to_asset_file_type(path)) {
        case AssetFileType::PNG:
        case AssetFileType::JPEG: {
            auto image = STBImageInfo::parse(file_contents);
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

auto AssetManager::import_scene(Asset *asset) -> bool {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

    auto &app = Application::get();
    auto &world = app.world;

    File file(asset->path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", asset->path);
        return false;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return false;
    }

    auto name_json = doc["name"].get_string();
    if (name_json.error()) {
        LOG_ERROR("Scene files must have names!");
        return false;
    }

    auto &scene_impl = impl->scenes.get(asset->scene_id);
    auto scene = Scene(scene_impl);
    scene.root().set_name(stack.null_terminate(name_json.value()).data());

    auto entities_json = doc["entities"].get_array();
    for (auto entity_json : entities_json) {
        auto entity_name_json = entity_json["name"];
        if (entity_name_json.error()) {
            LOG_ERROR("Entities must have names!");
            return false;
        }

        auto entity_name = entity_name_json.get_string().value();
        auto e = scene.create_entity(std::string(entity_name.begin(), entity_name.end()));

        auto entity_tags_json = entity_json["tags"];
        for (auto entity_tag : entity_tags_json.get_array()) {
            auto tag = world.ecs().component(stack.null_terminate(entity_tag.get_string()).data());
            e.add(tag);
        }

        auto components_json = entity_json["components"];
        for (auto component_json : components_json.get_array()) {
            auto component_name_json = component_json["name"];
            if (component_name_json.error()) {
                LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name());
                return false;
            }

            auto component_name = stack.null_terminate(component_name_json.get_string());
            auto component_id = world.ecs().lookup(component_name.data());
            if (!component_id) {
                LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
                return false;
            }

            e.add(component_id);
            Component::Wrapper component(e, component_id);
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                auto member_json = component_json[member_name];
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](f32 *v) { *v = static_cast<f32>(member_json.get_double()); },
                        [&](i32 *v) { *v = static_cast<i32>(member_json.get_int64()); },
                        [&](u32 *v) { *v = member_json.get_uint64(); },
                        [&](i64 *v) { *v = member_json.get_int64(); },
                        [&](u64 *v) { *v = member_json.get_uint64(); },
                        [&](glm::vec2 *v) { json_to_vec(member_json.value(), *v); },
                        [&](glm::vec3 *v) { json_to_vec(member_json.value(), *v); },
                        [&](glm::vec4 *v) { json_to_vec(member_json.value(), *v); },
                        [&](std::string *v) { *v = member_json.get_string().value(); },
                        [&](Identifier *v) { *v = member_json.get_string().value(); },
                    },
                    member);
            });
        }
    }

    return true;
}

auto AssetManager::export_scene(SceneID scene_id) -> bool {
    ZoneScoped;

    auto scene = this->get_scene(scene_id);
    LS_EXPECT(scene);
    auto *scene_asset = this->get_asset(scene.name_sv());
    LS_EXPECT(scene_asset);

    JsonWriter json;
    json.begin_obj();
    json["name"] = scene.name_sv();
    json["entities"].begin_array();
    scene.root().children([&](flecs::entity e) {
        json.begin_obj();
        json["name"] = std::string_view(e.name(), e.name().length());

        std::vector<Component::Wrapper> components = {};

        json["tags"].begin_array();
        e.each([&](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            Component::Wrapper component(e, component_id);
            if (!component.has_component()) {
                json << component.path;
            } else {
                components.emplace_back(e, component_id);
            }
        });
        json.end_array();

        json["components"].begin_array();
        for (auto &component : components) {
            json.begin_obj();
            json["name"] = component.path;
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                auto &member_json = json[member_name];
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](f32 *v) { member_json = *v; },
                        [&](i32 *v) { member_json = *v; },
                        [&](u32 *v) { member_json = *v; },
                        [&](i64 *v) { member_json = *v; },
                        [&](u64 *v) { member_json = *v; },
                        [&](glm::vec2 *v) { member_json = *v; },
                        [&](glm::vec3 *v) { member_json = *v; },
                        [&](glm::vec4 *v) { member_json = *v; },
                        [&](std::string *v) { member_json = *v; },
                        [&](Identifier *v) { member_json = v->sv().data(); },
                    },
                    member);
            });
            json.end_obj();
        }
        json.end_array();
        json.end_obj();
    });

    json.end_array();
    json.end_obj();

    File file(scene_asset->path, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", scene_asset->path);
        return false;
    }

    file.write(json.stream.view().data(), json.stream.view().length());

    return true;
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

auto AssetManager::get_scene(const Identifier &ident) -> Scene {
    ZoneScoped;

    auto *asset = this->get_asset(ident);
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
