#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/App.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Memory/Stack.hh"

namespace lr {
auto SceneRenderer::draw_for_camera(this SceneRenderer &, vuk::Value<vuk::Buffer> &camera_buffer, GeometryContext &context) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto &device = App::mod<Device>();
    auto &descriptor_set = device.get_descriptor_set();

    auto encode_pass = vuk::make_pass(
        stack.format("vis encode {}", context.late ? "late" : "early"),
        [&descriptor_set](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) triangle_indirect,
            VUK_BA(vuk::eIndexRead) index_buffer,
            VUK_BA(vuk::eVertexRead) camera,
            VUK_BA(vuk::eVertexRead) meshes,
            VUK_BA(vuk::eVertexRead) mesh_instances,
            VUK_BA(vuk::eVertexRead) meshlet_instances,
            VUK_BA(vuk::eVertexRead) transforms,
            VUK_BA(vuk::eFragmentRead) materials,
            VUK_IA(vuk::eColorRW) visbuffer,
            VUK_IA(vuk::eDepthStencilRW) depth,
            VUK_IA(vuk::eFragmentRW) overdraw
        ) {
            cmd_list //
                .bind_graphics_pipeline("passes.visbuffer_encode")
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual })
                .set_color_blend(visbuffer, vuk::BlendPreset::eOff)
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, camera)
                .bind_buffer(0, 1, meshes)
                .bind_buffer(0, 2, mesh_instances)
                .bind_buffer(0, 3, meshlet_instances)
                .bind_buffer(0, 4, transforms)
                .bind_buffer(0, 5, materials)
                .bind_image(0, 6, overdraw)
                .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                .bind_persistent(1, descriptor_set)
                .draw_indexed_indirect(1, triangle_indirect);

            return std::make_tuple(
                index_buffer, //
                camera,
                meshes,
                mesh_instances,
                meshlet_instances,
                transforms,
                materials,
                visbuffer,
                depth,
                overdraw
            );
        }
    );

    std::tie(
        context.reordered_indices_buffer,
        camera_buffer,
        context.meshes_buffer,
        context.mesh_instances_buffer,
        context.meshlet_instances_buffer,
        context.transforms_buffer,
        context.materials_buffer,
        context.visbuffer_attachment,
        context.depth_attachment,
        context.overdraw_attachment
    ) =
        encode_pass(
            std::move(context.draw_geometry_cmd_buffer),
            std::move(context.reordered_indices_buffer),
            std::move(camera_buffer),
            std::move(context.meshes_buffer),
            std::move(context.mesh_instances_buffer),
            std::move(context.meshlet_instances_buffer),
            std::move(context.transforms_buffer),
            std::move(context.materials_buffer),
            std::move(context.visbuffer_attachment),
            std::move(context.depth_attachment),
            std::move(context.overdraw_attachment)
        );
}

auto SceneRenderer::draw_depth_for_camera(this SceneRenderer &, vuk::Value<vuk::Buffer> &camera_buffer, GeometryContext &context) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto encode_pass = vuk::make_pass(
        stack.format("vis depth only {}", context.late ? "late" : "early"),
        [](vuk::CommandBuffer &cmd_list,
           VUK_BA(vuk::eIndirectRead) triangle_indirect,
           VUK_BA(vuk::eIndexRead) index_buffer,
           VUK_BA(vuk::eVertexRead) camera,
           VUK_BA(vuk::eVertexRead) meshes,
           VUK_BA(vuk::eVertexRead) mesh_instances,
           VUK_BA(vuk::eVertexRead) meshlet_instances,
           VUK_BA(vuk::eVertexRead) transforms,
           VUK_IA(vuk::eDepthStencilRW) depth) {
            cmd_list //
                .bind_graphics_pipeline("passes.visbuffer_depth_only")
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual })
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, camera)
                .bind_buffer(0, 1, meshes)
                .bind_buffer(0, 2, mesh_instances)
                .bind_buffer(0, 3, meshlet_instances)
                .bind_buffer(0, 4, transforms)
                .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                .draw_indexed_indirect(1, triangle_indirect);

            return std::make_tuple(
                index_buffer, //
                camera,
                meshes,
                mesh_instances,
                meshlet_instances,
                transforms,
                depth
            );
        }
    );

    std::tie(
        context.reordered_indices_buffer,
        camera_buffer,
        context.meshes_buffer,
        context.mesh_instances_buffer,
        context.meshlet_instances_buffer,
        context.transforms_buffer,
        context.depth_attachment
    ) =
        encode_pass(
            std::move(context.draw_geometry_cmd_buffer),
            std::move(context.reordered_indices_buffer),
            std::move(camera_buffer),
            std::move(context.meshes_buffer),
            std::move(context.mesh_instances_buffer),
            std::move(context.meshlet_instances_buffer),
            std::move(context.transforms_buffer),
            std::move(context.depth_attachment)
        );
}
} // namespace lr
