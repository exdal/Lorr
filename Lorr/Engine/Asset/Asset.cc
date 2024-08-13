#include "Asset.hh"

#include "Engine/Graphics/Device.hh"
#include "Engine/OS/OS.hh"

#include "AssetParser.hh"

namespace lr {
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

}  // namespace lr
