#include "Asset.hh"

#include "Engine/Asset/ParserGLTF.hh"

namespace lr {
struct MaterialTextureInfo {
    Identifier albedo_ident = {};
    Identifier normal_ident = {};
    Identifier emissive_ident = {};
};

template<>
struct Handle<AssetManager>::Impl {
    Device device = {};
    fs::path root_path = fs::current_path();

    ankerl::unordered_dense::map<Identifier, Model> model_map = {};
    ankerl::unordered_dense::map<Identifier, Texture> texture_map = {};
    ankerl::unordered_dense::map<Identifier, Material> material_map = {};
};

auto AssetManager::create(Device_H device) -> AssetManager {
    ZoneScoped;

    auto impl = new Impl;
    impl->device = device;
    auto self = AssetManager(impl);

    auto invalid_texture_id = Image::create(
                                  device,
                                  vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
                                  vk::Format::R8G8B8A8_UNORM,
                                  vk::ImageType::View2D,
                                  { 1024, 1024, 1 })
                                  .value();
    std::vector<u32> invalid_texture_data(1024 * 1024, 0xFFFFFFFF);
    impl->device.upload(
        invalid_texture_id, invalid_texture_data.data(), invalid_texture_data.size() * sizeof(u32), vk::ImageLayout::ShaderReadOnly);
    self.add_texure(
        INVALID_TEXTURE,
        invalid_texture_id,
        Sampler::create(
            device,
            vk::Filtering::Linear,
            vk::Filtering::Linear,
            vk::Filtering::Nearest,
            vk::SamplerAddressMode::Repeat,
            vk::SamplerAddressMode::Repeat,
            vk::SamplerAddressMode::Repeat,
            vk::CompareOp::Never)
            .value());

    return self;
}

auto AssetManager::destroy() -> void {
    ZoneScoped;

    LOG_TRACE("{} alive textures.", impl->texture_map.size());
    LOG_TRACE("{} alive materials.", impl->material_map.size());
    LOG_TRACE("{} alive models.", impl->model_map.size());

    for (auto &[ident, texture] : impl->texture_map) {
        LOG_INFO("Alive texture {}", ident.sv());
        impl->device.destroy_sampler(texture.sampler_id);
        impl->device.destroy_image(texture.image_id);
    }

    for (auto &[ident, model] : impl->model_map) {
        LOG_INFO("Alive model {}", ident.sv());
        for (auto &v : model.meshes) {
            impl->device.destroy_buffer(v.vertex_buffer_cpu);
            impl->device.destroy_buffer(v.index_buffer_cpu);
        }
    }
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
        case AssetType::Image:
            return root / "textures";
        case AssetType::Model:
            return root / "models";
        case AssetType::Font:
            return root / "fonts";
    }
}

auto AssetManager::add_texure(const Identifier &ident, ImageID image_id, SamplerID sampler_id) -> Texture * {
    ZoneScoped;

    auto [texture_it, texture_inserted] = impl->texture_map.try_emplace(ident);
    if (!texture_inserted) {
        return nullptr;
    }

    auto &texture = texture_it->second;
    texture.image_id = image_id;
    texture.sampler_id = sampler_id;

    return &texture;
}

auto AssetManager::load_model(const Identifier &ident, const fs::path &path) -> Model * {
    ZoneScoped;

    auto [model_it, model_inseted] = impl->model_map.try_emplace(ident);
    if (!model_inseted) {
        return nullptr;
    }

    auto &model = model_it->second;
    auto model_info = GLTFModelInfo::parse(impl->device, path);
    LS_DEFER(&) {
        model_info->destroy(impl->device);
    };

    if (!model_info.has_value()) {
        LOG_ERROR("Failed to parse Model '{}'!", path);
        return nullptr;
    }

    auto graphics_queue = impl->device.queue(vk::CommandType::Graphics);
    auto image_cmd_list = graphics_queue.begin();

    std::vector<SamplerID> samplers;
    samplers.reserve(model_info->samplers.size());
    for (auto &v : model_info->samplers) {
        samplers.emplace_back(Sampler::create(
                                  impl->device,
                                  v.min_filter,
                                  v.mag_filter,
                                  vk::Filtering::Linear,
                                  v.address_u,
                                  v.address_v,
                                  vk::SamplerAddressMode::ClampToBorder,
                                  vk::CompareOp::Always)
                                  .value());
    }

    std::vector<ImageID> images;
    images.reserve(model_info->images.size());
    for (auto &v : model_info->images) {
        auto image_id = Image::create(
                            impl->device,
                            vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst | vk::ImageUsage::TransferSrc,
                            v.format,
                            vk::ImageType::View2D,
                            v.extent,
                            vk::ImageAspectFlag::Color,
                            1,
                            static_cast<u32>(glm::floor(glm::log2(static_cast<f32>(ls::max(v.extent.width, v.extent.height)))) + 1))
                            .value();
        auto image = impl->device.image(image_id);
        image.set_name(v.name);

        image_cmd_list.image_transition({
            .dst_stage = vk::PipelineStage::Copy,
            .dst_access = vk::MemoryAccess::TransferWrite,
            .new_layout = vk::ImageLayout::TransferDst,
            .image_id = image_id,
        });

        vk::ImageCopyRegion first_copy_region = {
            .image_subresource_layer = { vk::ImageAspectFlag::Color, 0, 0, 1 },
            .image_extent = v.extent,
        };
        image_cmd_list.copy_buffer_to_image(v.buffer_id.value(), image_id, vk::ImageLayout::TransferDst, first_copy_region);

        // Mip chain generation
        image_cmd_list.image_transition({
            .src_stage = vk::PipelineStage::Copy,
            .src_access = vk::MemoryAccess::TransferWrite,
            .dst_stage = vk::PipelineStage::Blit,
            .dst_access = vk::MemoryAccess::TransferRead,
            .old_layout = vk::ImageLayout::TransferDst,
            .new_layout = vk::ImageLayout::TransferSrc,
            .image_id = image_id,
            .subresource_range = { vk::ImageAspectFlag::Color, 0, 1, 0, 1 },
        });

        i32 width = static_cast<i32>(image.extent().width);
        i32 height = static_cast<i32>(image.extent().height);
        for (u32 i = 1; i < image.mip_count(); i++) {
            image_cmd_list.image_transition({
                .dst_stage = vk::PipelineStage::Blit,
                .dst_access = vk::MemoryAccess::TransferWrite,
                .new_layout = vk::ImageLayout::TransferDst,
                .image_id = image_id,
                .subresource_range = { vk::ImageAspectFlag::Color, i, 1, 0, 1 },
            });

            vk::ImageBlit blit = {
                .src_subresource = { vk::ImageAspectFlag::Color, i - 1, 0, 1 },
                .src_offsets = { {}, { width >> (i - 1), height >> (i - 1), 1 } },
                .dst_subresource = { vk::ImageAspectFlag::Color, i, 0, 1 },
                .dst_offsets = { {}, { width >> i, height >> i, 1 } },
            };
            image_cmd_list.blit_image(
                image_id, vk::ImageLayout::TransferSrc, image_id, vk::ImageLayout::TransferDst, vk::Filtering::Linear, blit);

            image_cmd_list.image_transition({
                .src_stage = vk::PipelineStage::Blit,
                .src_access = vk::MemoryAccess::TransferWrite,
                .dst_stage = vk::PipelineStage::Blit,
                .dst_access = vk::MemoryAccess::TransferRead,
                .old_layout = vk::ImageLayout::TransferDst,
                .new_layout = vk::ImageLayout::TransferSrc,
                .image_id = image_id,
                .subresource_range = { vk::ImageAspectFlag::Color, i, 1, 0, 1 },
            });
        }

        image_cmd_list.image_transition({
            .src_stage = vk::PipelineStage::Blit,
            .src_access = vk::MemoryAccess::TransferRead,
            .dst_stage = vk::PipelineStage::FragmentShader,
            .dst_access = vk::MemoryAccess::ShaderRead,
            .old_layout = vk::ImageLayout::TransferSrc,
            .new_layout = vk::ImageLayout::ShaderReadOnly,
            .image_id = image_id,
            .subresource_range = { vk::ImageAspectFlag::Color, 0, image.mip_count(), 0, 1 },
        });

        images.push_back(image_id);
    }

    graphics_queue.end(image_cmd_list);
    graphics_queue.submit({}, {});
    graphics_queue.wait();
    for (auto &v : model_info->images) {
        if (v.buffer_id.has_value()) {
            impl->device.destroy_buffer(v.buffer_id.value());
            v.buffer_id.reset();
        }
    }

    std::vector<Identifier> texture_idents;
    texture_idents.reserve(model_info->textures.size());
    for (auto &v : model_info->textures) {
        auto new_ident = Identifier::random();
        auto [texture_it, texture_inserted] = impl->texture_map.try_emplace(new_ident);
        if (texture_inserted) {
            auto &texture = texture_it->second;
            texture.image_id = images[v.image_index.value()];
            texture.sampler_id = samplers[v.sampler_index.value()];

            texture_idents.push_back(texture_it->first);
        } else {
            LOG_ERROR("Failed to insert identifier '{}' into textures map, wat?", new_ident.sv());
            texture_idents.push_back(INVALID_TEXTURE);
        }
    }

    std::vector<Identifier> material_idents = {};
    for (auto &v : model_info->materials) {
        auto new_ident = Identifier::random();
        auto [material_it, material_inserted] = impl->material_map.try_emplace(new_ident);
        if (material_inserted) {
            auto &material = material_it->second;
            material.albedo_color = v.albedo_color;
            material.emissive_color = v.emissive_color;
            material.roughness_factor = v.roughness_factor;
            material.metallic_factor = v.metallic_factor;
            material.alpha_mode = v.alpha_mode;
            material.alpha_cutoff = v.alpha_cutoff;

            if (auto i = v.albedo_texture_index; i.has_value()) {
                material.albedo_texture_ident = texture_idents[i.value()];
            }
            if (auto i = v.normal_texture_index; i.has_value()) {
                material.normal_texture_ident = texture_idents[i.value()];
            }
            if (auto i = v.emissive_texture_index; i.has_value()) {
                material.emissive_texture_ident = texture_idents[i.value()];
            }

            // self.add_material(new_ident, material);
            material_idents.push_back(material_it->first);
        }
    }

    for (auto &v : model_info->primitives) {
        auto &primitive = model.primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset.value();
        primitive.vertex_count = v.vertex_count.value();
        primitive.index_offset = v.index_offset.value();
        primitive.index_count = v.index_count.value();
        primitive.material_ident = material_idents[v.material_index.value()];
    }

    for (auto &v : model_info->meshes) {
        auto &mesh = model.meshes.emplace_back();
        mesh.name = v.name;
        mesh.primitive_indices = v.primitive_indices;
        mesh.vertex_buffer_cpu = v.vertex_buffer_cpu.value();
        mesh.index_buffer_cpu = v.index_buffer_cpu.value();
        mesh.total_vertex_count = v.total_vertex_count;
        mesh.total_index_count = v.total_index_count;
        v.vertex_buffer_cpu.reset();
        v.index_buffer_cpu.reset();
    }

    return &model;
}

auto AssetManager::load_material(const Identifier &, const fs::path &) -> Material * {
    ZoneScoped;

    return nullptr;
}

auto AssetManager::model(const Identifier &ident) -> Model * {
    ZoneScoped;

    auto it = impl->model_map.find(ident);
    if (it == impl->model_map.end()) {
        return nullptr;
    }

    return &it->second;
}

auto AssetManager::texture(const Identifier &ident) -> Texture * {
    ZoneScoped;

    auto it = impl->texture_map.find(ident);
    if (it == impl->texture_map.end()) {
        return nullptr;
    }

    return &it->second;
}

auto AssetManager::material(const Identifier &ident) -> Material * {
    ZoneScoped;

    auto it = impl->material_map.find(ident);
    if (it == impl->material_map.end()) {
        return nullptr;
    }

    return &it->second;
}

}  // namespace lr
