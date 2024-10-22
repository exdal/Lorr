#include "Asset.hh"

#include "Engine/OS/OS.hh"

#include "Engine/Asset/AssetParser.hh"

namespace lr {
bool AssetManager::init(this AssetManager &self, Device device) {
    ZoneScoped;

    self.device = device;
    self.shader_compiler = SlangCompiler::create().value();

    u32 invalid_tex_data[] = { 0xFF000000, 0xFFFF00FF, 0xFFFF00FF, 0xFF000000 };
    // auto invalid_image_id = self.device->create_image(ImageInfo{
    //     .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
    //     .format = Format::R8G8B8A8_UNORM,
    //     .type = ImageType::View2D,
    //     .extent = { 2, 2, 1 },
    //     .debug_name = "Invalid Placeholder",
    // });
    // self.device->set_image_data(invalid_image_id, &invalid_tex_data, ImageLayout::ColorReadOnly);
    // auto invalid_image_view_id = self.device->create_image_view(ImageViewInfo{
    //     .image_id = invalid_image_id,
    //     .type = ImageViewType::View2D,
    //     .debug_name = "Invalid Placeholder View",
    // });
    // auto invalid_sampler_id = self.device->create_sampler({
    //     .min_filter = Filtering::Nearest,
    //     .mag_filter = Filtering::Nearest,
    //     .address_u = TextureAddressMode::Repeat,
    //     .address_v = TextureAddressMode::Repeat,
    // });
    // self.textures.try_emplace(INVALID_TEXTURE, invalid_image_id, invalid_image_view_id, invalid_sampler_id);

    return true;
}

void AssetManager::shutdown(this AssetManager &self, bool print_reports) {
    ZoneScoped;

    if (print_reports) {
        LOG_INFO("{} alive textures.", self.textures.size());
        LOG_INFO("{} alive materials.", self.materials.size());
        LOG_INFO("{} alive models.", self.models.size());
    }

    for (auto &[ident, texture] : self.textures) {
        LOG_INFO("Alive texture {}", ident.sv());
        // self.device->delete_images(texture.image_id);
        // self.device->delete_image_views(texture.image_view_id);
        // self.device->delete_samplers(texture.sampler_id);
    }

    for (auto &[ident, model] : self.models) {
        LOG_INFO("Alive model {}", ident.sv());
        // if (model.vertex_buffer.has_value()) {
        //     auto id = model.vertex_buffer.value();
        //     self.device->delete_buffers(id);
        // }
        // if (model.index_buffer.has_value()) {
        //     auto id = model.index_buffer.value();
        //     self.device->delete_buffers(id);
        // }
    }
}

ls::option<SlangModule> AssetManager::load_shader(
    this AssetManager &self, const ShaderSessionInfo &session_info, const ShaderCompileInfo &compile_info) {
    ZoneScoped;

    // TODO: Error handling
    auto compiler_session = self.shader_compiler.new_session(session_info).value();
    return compiler_session.load_module(compile_info).value();
}

Model *AssetManager::load_model(this AssetManager &self, const Identifier &ident, const fs::path &path) {
    ZoneScoped;

    Model model = {};
    auto model_data = AssetParser::GLTF(path);

    std::vector<Identifier> texture_idents;
    for (auto &v : model_data->textures) {
        std::unique_ptr<ImageAssetData> image_data;
        switch (v.file_type) {
            case AssetFileType::PNG:
            case AssetFileType::JPEG:
                image_data = AssetParser::STB(v.data.data(), v.data.size());
                break;
            default:;
        }

        if (image_data) {
            auto new_ident = Identifier::random();
            vk::Extent3D extent = { image_data->width, image_data->height, 1 };
            auto &texture = self.textures[new_ident];
            // TODO: Error handling
            auto image_id = Image::create(
                self.device,
                vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
                image_data->format,
                vk::ImageType::View2D,
                extent,
                vk::ImageAspectFlag::Color,
                1,
                1);
            auto image_view_id = ImageView::create(
                self.device,
                texture.image_id,
                vk::ImageViewType::View2D,
                vk::ImageSubresourceRange{
                    .aspect_flags = vk::ImageAspectFlag::Color,
                });
            self.device.upload(texture.image_id, image_data->data, image_data->data_size, vk::ImageLayout::ColorReadOnly);

            texture.image_id = image_id.value();
            texture.image_view_id = image_view_id.value();

            if (auto sampler_index = v.sampler_index; sampler_index.has_value()) {
                auto &sampler_info = model_data->samplers[sampler_index.value()];
                // TODO: Error handling
                auto sampler = Sampler::create(
                    self.device,
                    sampler_info.min_filter,
                    sampler_info.mag_filter,
                    vk::Filtering::Linear,
                    sampler_info.address_u,
                    sampler_info.address_v,
                    vk::SamplerAddressMode::Repeat,
                    vk::CompareOp::LessEqual);
                texture.sampler_id = sampler.value_or(SamplerID::Invalid);
            }

            texture_idents.push_back(new_ident);
        } else {
            LOG_ERROR("An image named {} could not be parsed!", v.name);
            texture_idents.push_back(INVALID_TEXTURE);
        }
    }

    std::vector<Identifier> material_idents;
    for (auto &v : model_data->materials) {
        auto new_ident = Identifier::random();

        Material material = {};
        material.albedo_color = v.albedo_color;
        material.emissive_color = v.emissive_color;
        material.roughness_factor = v.roughness_factor;
        material.metallic_factor = v.metallic_factor;
        material.alpha_mode = v.alpha_mode;
        material.alpha_cutoff = v.alpha_cutoff;

        if (auto i = v.albedo_image_data_index; i.has_value()) {
            material.albedo_texture = texture_idents[i.value()];
        }
        if (auto i = v.normal_image_data_index; i.has_value()) {
            material.normal_texture = texture_idents[i.value()];
        }
        if (auto i = v.emissive_image_data_index; i.has_value()) {
            material.emissive_texture = texture_idents[i.value()];
        }

        self.add_material(new_ident, material);
        material_idents.push_back(new_ident);
    }

    for (auto &v : model_data->primitives) {
        auto &primitive = model.primitives.emplace_back();
        primitive.vertex_offset = v.vertex_offset.value();
        primitive.vertex_count = v.vertex_count.value();
        primitive.index_offset = v.index_offset.value();
        primitive.index_count = v.index_count.value();

        if (auto i = v.material_index; i.has_value()) {
            primitive.material_ident = material_idents[i.value()];
        }
    }

    for (auto &v : model_data->meshes) {
        auto &mesh = model.meshes.emplace_back();
        mesh.name = v.name;
        mesh.primitive_indices = v.primitive_indices;
    }

    usize vertex_upload_size = model_data->vertices.size() * sizeof(Vertex);
    usize index_upload_size = model_data->indices.size() * sizeof(u32);
    auto vertex_buffer_id = Buffer::create(
                                self.device,
                                vk::BufferUsage::TransferDst | vk::BufferUsage::Vertex,
                                vertex_upload_size,
                                vk::MemoryAllocationUsage::PreferDevice,
                                vk::MemoryAllocationFlag::None)
                                .value();
    auto index_buffer_id = Buffer::create(
                               self.device,
                               vk::BufferUsage::TransferDst | vk::BufferUsage::Index,
                               index_upload_size,
                               vk::MemoryAllocationUsage::PreferDevice,
                               vk::MemoryAllocationFlag::None)
                               .value();

    return &self.models.try_emplace(ident, model).first->second;
}

Material *AssetManager::add_material(this AssetManager &self, const Identifier &ident, const Material &material_data) {
    ZoneScoped;

    auto material_iter = self.materials.try_emplace(ident, material_data);
    auto &material = material_iter.first->second;

    return &material;
}

Texture *AssetManager::texture_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.textures.find(ident);
    if (it == self.textures.end()) {
        return nullptr;
    }

    return &it->second;
}

Material *AssetManager::material_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.materials.find(ident);
    if (it == self.materials.end()) {
        return nullptr;
    }

    return &it->second;
}

Model *AssetManager::model_at(this AssetManager &self, const Identifier &ident) {
    ZoneScoped;

    auto it = self.models.find(ident);
    if (it == self.models.end()) {
        return nullptr;
    }

    return &it->second;
}

}  // namespace lr
