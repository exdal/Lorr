#include "Engine/Asset/ParserGLTF.hh"

#include "Engine/Asset/ParserSTB.hh"
#include "Engine/OS/File.hh"

#include <glm/packing.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

template<>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<glm::vec4, AccessorType::Vec4, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<glm::vec2, AccessorType::Vec2, float> {};

namespace lr {
auto GLTFModelInfo::parse(const fs::path &path, bool load_resources) -> ls::option<GLTFModelInfo> {
    ZoneScoped;

    auto gltf_buffer = fastgltf::GltfDataBuffer::FromPath(path);
    auto gltf_type = fastgltf::determineGltfFileType(gltf_buffer.get());
    if (gltf_type == fastgltf::GltfType::Invalid) {
        LOG_ERROR("GLTF model type is invalid!");
        return ls::nullopt;
    }

    auto extensions = fastgltf::Extensions::None;
    extensions |= fastgltf::Extensions::KHR_mesh_quantization;
    extensions |= fastgltf::Extensions::KHR_texture_transform;
    extensions |= fastgltf::Extensions::KHR_texture_basisu;
    extensions |= fastgltf::Extensions::KHR_lights_punctual;
    extensions |= fastgltf::Extensions::KHR_materials_specular;
    extensions |= fastgltf::Extensions::KHR_materials_ior;
    extensions |= fastgltf::Extensions::KHR_materials_iridescence;
    extensions |= fastgltf::Extensions::KHR_materials_volume;
    extensions |= fastgltf::Extensions::KHR_materials_transmission;
    extensions |= fastgltf::Extensions::KHR_materials_clearcoat;
    extensions |= fastgltf::Extensions::KHR_materials_emissive_strength;
    extensions |= fastgltf::Extensions::KHR_materials_sheen;
    extensions |= fastgltf::Extensions::KHR_materials_unlit;
    extensions |= fastgltf::Extensions::KHR_materials_anisotropy;
    extensions |= fastgltf::Extensions::EXT_meshopt_compression;
    extensions |= fastgltf::Extensions::EXT_texture_webp;
    extensions |= fastgltf::Extensions::MSFT_texture_dds;

    fastgltf::Parser parser;

    auto options = fastgltf::Options::None;
    // options |= fastgltf::Options::DontRequireValidAssetMember;

    if (load_resources) {
        options |= fastgltf::Options::LoadExternalBuffers;
    }

    auto result = parser.loadGltf(gltf_buffer.get(), path.parent_path(), options);
    if (!result) {
        LOG_ERROR("Failed to load GLTF! {}", fastgltf::getErrorMessage(result.error()));
        return ls::nullopt;
    }

    fastgltf::Asset asset = std::move(result.get());
    GLTFModelInfo model = {};

    ///////////////////////////////////////////////
    // Buffers
    ///////////////////////////////////////////////

    std::vector<ls::span<u8>> buffers = {};

    // sources::Vector is not used for importing, ignore it
    for (const auto &v : asset.buffers) {
        if (!load_resources) {
            buffers.emplace_back();
            continue;
        }

        std::visit(
            fastgltf::visitor{
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded byte
                    buffers.emplace_back(ls::bit_cast<u8 *>(view.bytes.data()), view.bytes.size_bytes());
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    buffers.emplace_back(ls::bit_cast<u8 *>(arr.bytes.data()), arr.bytes.size_bytes());
                },
            },
            v.data);
    }

    ///////////////////////////////////////////////
    // Samplers
    ///////////////////////////////////////////////

    auto gltf_filter_to_lr = [](fastgltf::Filter f) -> vuk::Filter {
        switch (f) {
            case fastgltf::Filter::Nearest:
                return vuk::Filter::eNearest;
            case fastgltf::Filter::Linear:
            default:
                return vuk::Filter::eLinear;
        }
    };

    auto gltf_address_mode_to_lr = [](fastgltf::Wrap w) -> vuk::SamplerAddressMode {
        switch (w) {
            case fastgltf::Wrap::ClampToEdge:
                return vuk::SamplerAddressMode::eClampToEdge;
            case fastgltf::Wrap::MirroredRepeat:
                return vuk::SamplerAddressMode::eMirroredRepeat;
            case fastgltf::Wrap::Repeat:
                return vuk::SamplerAddressMode::eRepeat;
        }
    };

    for (const auto &v : asset.samplers) {
        auto &sampler = model.samplers.emplace_back();
        sampler.mag_filter = gltf_filter_to_lr(v.magFilter.value_or(fastgltf::Filter::Linear));
        sampler.min_filter = gltf_filter_to_lr(v.minFilter.value_or(fastgltf::Filter::Linear));
        sampler.address_u = gltf_address_mode_to_lr(v.wrapS);
        sampler.address_v = gltf_address_mode_to_lr(v.wrapT);
    }

    ///////////////////////////////////////////////
    // Images
    ///////////////////////////////////////////////

    auto parse_image = [&](const std::string &name, ls::span<u8> data, fastgltf::MimeType mime) -> ls::option<GLTFImageInfo> {
        GLTFImageInfo info = {};
        info.name = name;
        switch (mime) {
            case fastgltf::MimeType::JPEG:
            case fastgltf::MimeType::PNG: {
                auto image = STBImageInfo::parse(data);
                if (!image.has_value()) {
                    return ls::nullopt;
                }
                info.format = image->format;
                info.extent = image->extent;
                info.pixels = std::move(image->data);
                return info;
            }
            case fastgltf::MimeType::KTX2:
            case fastgltf::MimeType::DDS: {
            }
            case fastgltf::MimeType::None:
            case fastgltf::MimeType::GltfBuffer:
            case fastgltf::MimeType::OctetStream:
                return ls::nullopt;
        }
    };

    for (const auto &v : asset.images) {
        if (!load_resources) {
            model.images.emplace_back();
            continue;
        }

        std::visit(
            fastgltf::visitor{
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded buffer
                    auto sp = ls::span(ls::bit_cast<u8 *>(view.bytes.data()), view.bytes.size_bytes());
                    model.images.emplace_back(parse_image(std::string(v.name), sp, view.mimeType).value());
                },
                [&](const fastgltf::sources::BufferView &view) {
                    // Embedded buffer
                    auto &buffer_view = asset.bufferViews[view.bufferViewIndex];
                    auto &buffer = buffers[buffer_view.bufferIndex];
                    auto sp = ls::span(ls::bit_cast<u8 *>(buffer.data() + buffer_view.byteOffset), buffer_view.byteLength);
                    model.images.emplace_back(parse_image(std::string(v.name), sp, view.mimeType).value());
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    auto sp = ls::span(ls::bit_cast<u8 *>(arr.bytes.data()), arr.bytes.size_bytes());
                    model.images.emplace_back(parse_image(std::string(v.name), sp, arr.mimeType).value());
                },
                [&](const fastgltf::sources::URI &uri) {
                    // External file
                    const auto image_file_path = path.parent_path() / uri.uri.fspath();
                    File file(image_file_path, FileAccess::Read);
                    file.seek(static_cast<i64>(uri.fileByteOffset));
                    std::vector<u8> file_data(file.size - uri.fileByteOffset);
                    file.read(file_data.data(), file_data.size());
                    model.images.emplace_back(parse_image(std::string(v.name), file_data, fastgltf::MimeType::PNG).value());
                },
            },
            v.data);
    }

    ///////////////////////////////////////////////
    // Textures
    ///////////////////////////////////////////////

    for (const auto &v : asset.textures) {
        auto &texture = model.textures.emplace_back();
        if (v.samplerIndex.has_value()) {
            texture.sampler_index = v.samplerIndex.value();
        }
        if (v.imageIndex.has_value()) {
            texture.image_index = v.imageIndex.value();
        }
    }

    ///////////////////////////////////////////////
    // Materials
    ///////////////////////////////////////////////

    for (const auto &v : asset.materials) {
        auto &material = model.materials.emplace_back();
        auto &pbr = v.pbrData;

        material.albedo_color = {
            pbr.baseColorFactor[0],
            pbr.baseColorFactor[1],
            pbr.baseColorFactor[2],
            pbr.baseColorFactor[3],
        };
        material.metallic_factor = pbr.metallicFactor;
        material.roughness_factor = pbr.roughnessFactor;
        material.emissive_color = {
            v.emissiveFactor[0],
            v.emissiveFactor[1],
            v.emissiveFactor[2],
            v.emissiveStrength,
        };
        // material.alpha_mode = static_cast<vk::AlphaMode>(v.alphaMode);
        material.alpha_cutoff = v.alphaCutoff;

        if (auto &tex = pbr.baseColorTexture; tex.has_value()) {
            material.albedo_texture_index = tex->textureIndex;
        }

        if (auto &tex = v.normalTexture; tex.has_value()) {
            material.normal_texture_index = tex->textureIndex;
        }

        if (auto &tex = v.emissiveTexture; tex.has_value()) {
            material.emissive_texture_index = tex->textureIndex;
        }
    }

    u32 total_vertex_count = 0;
    u32 total_index_count = 0;
    for (const auto &v : asset.meshes) {
        for (const auto &k : v.primitives) {
            auto &index_accessor = asset.accessors[k.indicesAccessor.value()];
            if (auto attrib = k.findAttribute("POSITION"); attrib != k.attributes.end()) {
                auto &vertex_accessor = asset.accessors[attrib->accessorIndex];
                total_vertex_count += vertex_accessor.count;
            }

            total_index_count += index_accessor.count;
        }
    }

    if (load_resources) {
        model.vertices.resize(total_vertex_count);
        model.indices.resize(total_index_count);

        u32 vertex_offset = 0;
        u32 index_offset = 0;
        for (const auto &v : asset.meshes) {
            auto &mesh = model.meshes.emplace_back();
            mesh.name = v.name;

            for (const auto &k : v.primitives) {
                mesh.primitive_indices.push_back(model.primitives.size());
                auto &primitive = model.primitives.emplace_back();
                auto &index_accessor = asset.accessors[k.indicesAccessor.value()];

                primitive.vertex_offset = vertex_offset;
                primitive.index_offset = index_offset;
                primitive.index_count = index_accessor.count;

                fastgltf::iterateAccessorWithIndex<u32>(asset, index_accessor, [&](u32 index, usize i) {  //
                    model.indices[index_offset + i] = index;
                });

                if (auto attrib = k.findAttribute("POSITION"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    primitive.vertex_count = accessor.count;

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, usize i) {  //
                        model.vertices[vertex_offset + i].position = pos;
                    });
                }

                if (auto attrib = k.findAttribute("NORMAL"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, usize i) {  //
                        model.vertices[vertex_offset + i].normal = normal;
                    });
                }

                if (auto attrib = k.findAttribute("TEXCOORD_0"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 uv, usize i) {  //
                        model.vertices[vertex_offset + i].tex_coord_0 = uv;
                    });
                }

                if (auto attrib = k.findAttribute("COLOR"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor, [&](glm::vec4 color, usize i) {  //
                        model.vertices[vertex_offset + i].color = glm::packUnorm4x8(color);
                    });
                }

                if (k.materialIndex.has_value()) {
                    primitive.material_index = k.materialIndex.value();
                }

                vertex_offset += primitive.vertex_count;
                index_offset += primitive.index_count;
            }
        }
    }

    return model;
}

}  // namespace lr
