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

ls::option<ModelID> AssetManager::load_model(this AssetManager &self, std::string_view path) {
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
        auto &material = self.materials.emplace_back();
        material.albedo_color = v.albedo_color;
        material.emissive_color = v.emissive_color;
        material.roughness_factor = v.roughness_factor;
        material.metallic_factor = v.metallic_factor;
        material.alpha_mode = v.alpha_mode;
        material.alpha_cutoff = v.alpha_cutoff;

        // TODO: When there is a potential error, we need an error texture
        // or full white texture.
        if (auto i = v.albedo_image_data_index; i.has_value()) {
            material.albedo_texture_index = i.value() + texture_offset;
        }
        if (auto i = v.normal_image_data_index; i.has_value()) {
            material.normal_texture_index = i.value() + texture_offset;
        }
        if (auto i = v.emissive_image_data_index; i.has_value()) {
            material.emissive_texture_index = i.value() + texture_offset;
        }
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

    self.refresh_materials();
    return static_cast<ModelID>(model_id);
}

void AssetManager::refresh_materials(this AssetManager &self) {
    ZoneScoped;

    LR_ASSERT(self.materials.size() < MAX_MATERIAL_COUNT, "Material count must be lesser than MAX_MATERIAL_COUNT!");

    std::vector<GPUMaterial> gpu_materials = {};
    for (auto &v : self.materials) {
        auto &material = gpu_materials.emplace_back();

        material.albedo_color = v.albedo_color;
        material.emissive_color = v.emissive_color;
        material.roughness_factor = v.roughness_factor;
        material.metallic_factor = v.metallic_factor;
        material.alpha_mode = v.alpha_mode;
        material.alpha_cutoff = v.alpha_cutoff;
        {
            auto &texture = self.textures[v.albedo_texture_index.value_or(0)];
            material.albedo_image_view = texture.image_view_id;
            material.albedo_sampler = texture.sampler_id;
        }
        {
            auto &texture = self.textures[v.normal_texture_index.value_or(0)];
            material.normal_image_view = texture.image_view_id;
            material.normal_sampler = texture.sampler_id;
        }
        {
            auto &texture = self.textures[v.emissive_texture_index.value_or(0)];
            material.emissive_image_view = texture.image_view_id;
            material.emissive_sampler = texture.sampler_id;
        }
    }

    auto &staging_buffer = self.device->staging_buffer_at(0);
    staging_buffer.reset();
    auto buffer_alloc = staging_buffer.alloc(gpu_materials.size() * sizeof(GPUMaterial));
    std::memcpy(buffer_alloc.ptr, gpu_materials.data(), gpu_materials.size() * sizeof(GPUMaterial));

    auto &queue = self.device->queue_at(CommandType::Transfer);
    auto cmd_list = queue.begin_command_list(0);
    BufferCopyRegion copy_region = { .src_offset = buffer_alloc.offset, .dst_offset = 0, .size = buffer_alloc.size };
    cmd_list.copy_buffer_to_buffer(buffer_alloc.buffer_id, self.material_buffer_id, copy_region);
    queue.end_command_list(cmd_list);
    queue.submit(0, {});
    queue.wait_for_work();
}

}  // namespace lr
