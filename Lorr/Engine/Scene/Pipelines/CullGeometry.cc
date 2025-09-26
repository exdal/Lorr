#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/App.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Memory/Stack.hh"

namespace lr {
static constexpr auto sampler_min_clamp_reduction_mode = VkSamplerReductionModeCreateInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
    .pNext = nullptr,
    .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
};

static constexpr auto hiz_sampler = vuk::SamplerCreateInfo{
    .pNext = &sampler_min_clamp_reduction_mode,
    .magFilter = vuk::Filter::eLinear,
    .minFilter = vuk::Filter::eLinear,
    .mipmapMode = vuk::SamplerMipmapMode::eNearest,
    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
};

auto SceneRenderer::generate_hiz(this SceneRenderer &, GeometryContext &context) -> void {
    ZoneScoped;

    auto hiz_generate_slow_pass = vuk::make_pass(
        "hiz generate slow",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eComputeSampled) src,
           VUK_IA(vuk::eComputeRW) dst) {
            auto extent = dst->extent;
            auto mip_count = dst->level_count;

            cmd_list //
                .bind_compute_pipeline("passes.hiz_slow")
                .bind_sampler(0, 0, hiz_sampler);

            for (auto i = 0_u32; i < mip_count; i++) {
                auto mip_width = std::max(1_u32, extent.width >> i);
                auto mip_height = std::max(1_u32, extent.height >> i);

                auto mip = dst->mip(i);
                if (i == 0) {
                    cmd_list.bind_image(0, 1, src);
                } else {
                    auto mip = dst->mip(i - 1);
                    cmd_list.image_barrier(mip, vuk::eComputeWrite, vuk::eComputeSampled);
                    cmd_list.bind_image(0, 1, mip);
                }

                cmd_list.bind_image(0, 2, mip);
                cmd_list.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mip_width, mip_height, i));
                cmd_list.dispatch_invocations(mip_width, mip_height);
            }

            cmd_list.image_barrier(dst, vuk::eComputeSampled, vuk::eComputeRW);

            return std::make_tuple(src, dst);
        }
    );

    std::tie(context.depth_attachment, context.hiz_attachment) =
        hiz_generate_slow_pass(std::move(context.depth_attachment), std::move(context.hiz_attachment));
}

auto SceneRenderer::cull_for_camera(this SceneRenderer &, vuk::Value<vuk::Buffer> &camera_buffer, GeometryContext &context) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto &device = App::mod<Device>();
    auto &transfer_man = device.transfer_man();

    if (!context.late) {
        auto cull_meshes_pass = vuk::make_pass(
            "cull meshes",
            [mesh_instance_count = context.mesh_instance_count, cull_flags = context.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_BA(vuk::eComputeRead) meshes,
                VUK_BA(vuk::eComputeRW) mesh_instances,
                VUK_BA(vuk::eComputeRW) meshlet_instances,
                VUK_BA(vuk::eComputeRW) visibility,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRW) debug_drawer
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_meshes")
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, meshes)
                    .bind_buffer(0, 2, mesh_instances)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, visibility)
                    .bind_buffer(0, 5, transforms)
                    .bind_buffer(0, 6, debug_drawer)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mesh_instance_count, cull_flags))
                    .dispatch_invocations(mesh_instance_count);

                return std::make_tuple(camera, meshes, mesh_instances, meshlet_instances, visibility, transforms, debug_drawer);
            }
        );

        auto generate_cull_commands_pass = vuk::make_pass(
            "generate cull commands",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_BA(vuk::eComputeRead) visibility,
               VUK_BA(vuk::eComputeRW) cull_meshlets_cmd) {
                cmd_list //
                    .bind_compute_pipeline("passes.generate_cull_commands")
                    .bind_buffer(0, 0, visibility)
                    .bind_buffer(0, 1, cull_meshlets_cmd)
                    .dispatch(1);

                return std::make_tuple(visibility, cull_meshlets_cmd);
            }
        );

        context.visibility_buffer = transfer_man.scratch_buffer<MeshletInstanceVisibility>({});
        std::tie(
            camera_buffer,
            context.meshes_buffer,
            context.mesh_instances_buffer,
            context.meshlet_instances_buffer,
            context.visibility_buffer,
            context.transforms_buffer,
            context.debug_drawer_buffer
        ) =
            cull_meshes_pass(
                std::move(camera_buffer),
                std::move(context.meshes_buffer),
                std::move(context.mesh_instances_buffer),
                std::move(context.meshlet_instances_buffer),
                std::move(context.visibility_buffer),
                std::move(context.transforms_buffer),
                std::move(context.debug_drawer_buffer)
            );

        context.cull_meshlets_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        std::tie(context.visibility_buffer, context.cull_meshlets_cmd_buffer) =
            generate_cull_commands_pass(std::move(context.visibility_buffer), std::move(context.cull_meshlets_cmd_buffer));
    }

    auto cull_meshlets_pass = vuk::make_pass(
        stack.format("cull meshlets {}", context.late ? "late" : "early"),
        [late = context.late, cull_flags = context.cull_flags](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) dispatch_cmd,
            VUK_BA(vuk::eComputeRead) camera,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeRead) mesh_instances,
            VUK_BA(vuk::eComputeRead) meshlet_instances,
            VUK_BA(vuk::eComputeRW) visibility,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_BA(vuk::eComputeRW) visible_meshlet_instances_indices,
            VUK_BA(vuk::eComputeRW) meshlet_instance_visibility_mask,
            VUK_BA(vuk::eComputeRW) cull_triangles_cmd,
            VUK_IA(vuk::eComputeSampled) hiz
        ) {
            cmd_list //
                .bind_compute_pipeline("passes.cull_meshlets")
                .bind_image(0, 0, hiz)
                .bind_sampler(0, 1, hiz_sampler)
                .bind_buffer(0, 2, camera)
                .bind_buffer(0, 3, meshes)
                .bind_buffer(0, 4, mesh_instances)
                .bind_buffer(0, 5, meshlet_instances)
                .bind_buffer(0, 6, visibility)
                .bind_buffer(0, 7, transforms)
                .bind_buffer(0, 8, visible_meshlet_instances_indices)
                .bind_buffer(0, 9, meshlet_instance_visibility_mask)
                .bind_buffer(0, 10, cull_triangles_cmd)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, cull_flags)
                .specialize_constants(0, late)
                .dispatch_indirect(dispatch_cmd);

            return std::make_tuple(
                dispatch_cmd,
                camera,
                meshes,
                mesh_instances,
                meshlet_instances,
                visibility,
                transforms,
                visible_meshlet_instances_indices,
                meshlet_instance_visibility_mask,
                cull_triangles_cmd,
                hiz
            );
        }
    );

    auto cull_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });

    std::tie(
        context.cull_meshlets_cmd_buffer,
        camera_buffer,
        context.meshes_buffer,
        context.mesh_instances_buffer,
        context.meshlet_instances_buffer,
        context.visibility_buffer,
        context.transforms_buffer,
        context.visible_meshlet_instances_indices_buffer,
        context.meshlet_instance_visibility_mask_buffer,
        cull_triangles_cmd_buffer,
        context.hiz_attachment
    ) =
        cull_meshlets_pass(
            std::move(context.cull_meshlets_cmd_buffer),
            std::move(camera_buffer),
            std::move(context.meshes_buffer),
            std::move(context.mesh_instances_buffer),
            std::move(context.meshlet_instances_buffer),
            std::move(context.visibility_buffer),
            std::move(context.transforms_buffer),
            std::move(context.visible_meshlet_instances_indices_buffer),
            std::move(context.meshlet_instance_visibility_mask_buffer),
            std::move(cull_triangles_cmd_buffer),
            std::move(context.hiz_attachment)
        );

    auto cull_triangles_pass = vuk::make_pass(
        stack.format("cull triangles {}", context.late ? "late" : "early"),
        [late = context.late, cull_flags = context.cull_flags](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) cull_triangles_cmd,
            VUK_BA(vuk::eComputeRead) camera,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeRead) mesh_instances,
            VUK_BA(vuk::eComputeRead) meshlet_instances,
            VUK_BA(vuk::eComputeRead) visibility,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
            VUK_BA(vuk::eComputeRW) reordered_indices,
            VUK_BA(vuk::eComputeRW) draw_indexed_cmd
        ) {
            cmd_list //
                .bind_compute_pipeline("passes.cull_triangles")
                .bind_buffer(0, 0, camera)
                .bind_buffer(0, 1, meshes)
                .bind_buffer(0, 2, mesh_instances)
                .bind_buffer(0, 3, meshlet_instances)
                .bind_buffer(0, 4, visibility)
                .bind_buffer(0, 5, transforms)
                .bind_buffer(0, 6, visible_meshlet_instances_indices)
                .bind_buffer(0, 7, reordered_indices)
                .bind_buffer(0, 8, draw_indexed_cmd)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, cull_flags)
                .specialize_constants(0, late)
                .dispatch_indirect(cull_triangles_cmd);

            return std::make_tuple(
                camera,
                meshes,
                mesh_instances,
                meshlet_instances,
                visibility,
                transforms,
                visible_meshlet_instances_indices,
                reordered_indices,
                draw_indexed_cmd
            );
        }
    );

    context.draw_geometry_cmd_buffer = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
    std::tie(
        camera_buffer,
        context.meshes_buffer,
        context.mesh_instances_buffer,
        context.meshlet_instances_buffer,
        context.visibility_buffer,
        context.transforms_buffer,
        context.visible_meshlet_instances_indices_buffer,
        context.reordered_indices_buffer,
        context.draw_geometry_cmd_buffer
    ) =
        cull_triangles_pass(
            std::move(cull_triangles_cmd_buffer),
            std::move(camera_buffer),
            std::move(context.meshes_buffer),
            std::move(context.mesh_instances_buffer),
            std::move(context.meshlet_instances_buffer),
            std::move(context.visibility_buffer),
            std::move(context.transforms_buffer),
            std::move(context.visible_meshlet_instances_indices_buffer),
            std::move(context.reordered_indices_buffer),
            std::move(context.draw_geometry_cmd_buffer)
        );
}

} // namespace lr
