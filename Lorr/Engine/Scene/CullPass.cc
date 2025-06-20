#include "Engine/Scene/Passes.hh"

#include "Engine/Asset/Model.hh"

#include <vuk/RenderGraph.hpp>
#include <vuk/runtime/CommandBuffer.hpp>

namespace lr {
auto CullPass::setup(this CullPass &, Device &device, SlangSession &slang_session) -> void {
    ZoneScoped;

    // auto vis_cull_meshlets_pipeline_info = PipelineCompileInfo{
    //     .module_name = "passes.cull_meshlets",
    //     .entry_points = { "cs_main" },
    // };
    // Pipeline::create(device, slang_session, vis_cull_meshlets_pipeline_info).value();
    //
    // auto vis_cull_triangles_pipeline_info = PipelineCompileInfo{
    //     .module_name = "passes.cull_triangles",
    //     .entry_points = { "cs_main" },
    // };
    // Pipeline::create(device, slang_session, vis_cull_triangles_pipeline_info).value();
}

auto CullPass::run(this CullPass &self, Device &device) -> Outputs {
    ZoneScoped;

    auto &transfer_man = device.transfer_man();

    auto cull_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
    auto visible_meshlet_instances_indices_buffer =
        transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));
    auto draw_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });

    struct PassInfo {
        u32 meshlet_instance_count = self.meshlet_instance_count;
        GPU::CullFlags cull_flags = self.cull_flags;
    };

    auto pass_info = PassInfo{
        .meshlet_instance_count = self.meshlet_instance_count,
        .cull_flags = self.cull_flags,
    };

    auto vis_cull_meshlets_pass = vuk::make_pass(
        "vis cull meshlets",
        [pass_info](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::eComputeSampled) hiz,
            VUK_BA(vuk::eComputeWrite) cull_triangles_cmd,
            VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices,
            VUK_BA(vuk::eComputeRead) meshlet_instances,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeRead) camera
        ) {
            cmd_list //
                .bind_compute_pipeline("passes.cull_meshlets")
                //.bind_image(0, 0, hiz)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants(
                        cull_triangles_cmd->device_address,
                        visible_meshlet_instances_indices->device_address,
                        meshlet_instances->device_address,
                        transforms->device_address,
                        meshes->device_address,
                        camera->device_address,
                        pass_info.meshlet_instance_count,
                        pass_info.cull_flags
                    )
                )
                .dispatch((pass_info.meshlet_instance_count + Model::MAX_MESHLET_INDICES - 1) / Model::MAX_MESHLET_INDICES);
            return std::make_tuple(hiz, cull_triangles_cmd, visible_meshlet_instances_indices, meshlet_instances, transforms, meshes, camera);
        }
    );

    auto vis_cull_triangles_pass = vuk::make_pass(
        "vis cull triangles",
        [pass_info](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) cull_triangles_cmd,
            VUK_BA(vuk::eComputeWrite) draw_indexed_cmd,
            VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
            VUK_BA(vuk::eComputeWrite) reordered_indices,
            VUK_BA(vuk::eComputeRead) meshlet_instances,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeWrite) camera
        ) {
            cmd_list //
                .bind_compute_pipeline("passes.cull_triangles")
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants(
                        draw_indexed_cmd->device_address,
                        visible_meshlet_instances_indices->device_address,
                        reordered_indices->device_address,
                        meshlet_instances->device_address,
                        transforms->device_address,
                        meshes->device_address,
                        camera->device_address,
                        pass_info.cull_flags
                    )
                )
                .dispatch_indirect(cull_triangles_cmd);
            return std::make_tuple(
                draw_indexed_cmd,
                visible_meshlet_instances_indices,
                reordered_indices,
                meshlet_instances,
                transforms,
                meshes,
                camera
            );
        }
    );
}
} // namespace lr
