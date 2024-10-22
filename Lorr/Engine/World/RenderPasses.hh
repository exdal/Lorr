#pragma once

#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/Graphics/Slang/Compiler.hh"

namespace lr {
struct TaskExecutionData {
    u64 world_buffer_ptr = 0;
    SamplerID linear_sampler = SamplerID::Invalid;
};

struct RenderPassSetupContext {
    Device device = {};
    SlangModule shader_module = {};
    vk::Format swap_chain_format = {};
    vk::Extent3D swap_chain_extent = {};

    TaskImageID swap_chain_task_image = TaskImageID::Invalid;
    ImageViewID swap_chain_view = ImageViewID::Invalid;
};

struct RenderPassRecordInfo {
    TaskGraph *task_graph = nullptr;

    TaskImageID swap_chain_task_image = TaskImageID::Invalid;
    ImageViewID swap_chain_view = ImageViewID::Invalid;
};

struct RenderPass {
    std::string name = {};
    PipelineID pipeline = PipelineID::Invalid;

    virtual bool on_add(RenderPassSetupContext &ctx) = 0;
    virtual void on_remove() = 0;

    // Render state
    virtual void on_record(RenderPassRecordInfo &info) = 0;
    virtual bool on_surface_lost() = 0;
};

struct ImguiPass : RenderPass {
    ImFont *roboto_font = nullptr;
    ImFont *icons_font = nullptr;

    ImageID font_atlas_image = {};
    ImageViewID font_atlas_view = {};

    bool on_add(RenderPassSetupContext &ctx) override {
        name = "ImGui Pass";

        auto &imgui = ImGui::GetIO();
        auto roboto_path = (fs::current_path() / "resources/fonts/Roboto-Regular.ttf").string();
        auto fa_solid_900_path = (fs::current_path() / "resources/fonts/fa-solid-900.ttf").string();

        ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
        ImFontConfig font_config;
        font_config.GlyphMinAdvanceX = 16.0f;
        font_config.MergeMode = true;
        font_config.PixelSnapH = true;

        imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
        roboto_font = imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);

        font_config.GlyphMinAdvanceX = 32.0f;
        font_config.MergeMode = false;
        icons_font = imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 32.0f, &font_config, icons_ranges);

        imgui.Fonts->Build();

        u8 *font_data = nullptr;  // imgui context frees this itself
        i32 font_width, font_height;
        imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

        font_atlas_image = Image::create(
                               ctx.device,
                               vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
                               vk::Format::R8G8B8A8_UNORM,
                               vk::ImageType::View2D,
                               vk::Extent3D(static_cast<u32>(font_width), static_cast<u32>(font_height), 1u))
                               .value();
        font_atlas_view = ImageView::create(ctx.device, font_atlas_image, vk::ImageViewType::View2D, {}).value();
        ctx.device.upload(font_atlas_image, font_data, font_width * font_height * 4, vk::ImageLayout::ColorReadOnly);
        imgui.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<iptr>(font_atlas_view)));
        imgui.FontDefault = roboto_font;

        static std::vector<vk::VertexAttribInfo> IMGUI_VERTEX_LAYOUT = {
            { .format = vk::Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
            { .format = vk::Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
            { .format = vk::Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
        };

        auto vs_shader = Shader::create(ctx.device, ctx.shader_module.get_entry_point("vs_main"), vk::ShaderStageFlag::Vertex).value();
        auto fs_shader = Shader::create(ctx.device, ctx.shader_module.get_entry_point("fs_main"), vk::ShaderStageFlag::Fragment).value();
        auto reflection = ctx.shader_module.get_reflection();

        pipeline = Pipeline::create(
                       ctx.device,
                       GraphicsPipelineInfo{
                           .color_attachment_formats = { ctx.swap_chain_format },
                           .shaders = { vs_shader, fs_shader },
                           .vertex_attrib_infos = IMGUI_VERTEX_LAYOUT,
                           .depth_stencil_state = { .enable_depth_test = true,
                                                    .enable_depth_write = true,
                                                    .depth_compare_op = vk::CompareOp::LessEqual },
                           .blend_attachments = { {
                               .blend_enabled = true,
                               .src_blend = vk::BlendFactor::SrcAlpha,
                               .dst_blend = vk::BlendFactor::InvSrcAlpha,
                               .blend_op = vk::BlendOp::Add,
                               .src_blend_alpha = vk::BlendFactor::One,
                               .dst_blend_alpha = vk::BlendFactor::InvSrcAlpha,
                               .blend_op_alpha = vk::BlendOp::Add,
                           } },
                           .layout_id = reflection.pipeline_layout_id,
                       })
                       .value();

        return true;
    }

    void on_remove() override {}

    void on_record(RenderPassRecordInfo &info) override {
        info.task_graph->add_task({
            .name = "ImGui Task",
            .uses = {
                Preset::ColorAttachmentWrite(info.swap_chain_task_image),
            },
            .execute_cb = [&](TaskContext &tc) {
                auto &exec_data = tc.exec_data_as<TaskExecutionData>();
                auto &imgui = ImGui::GetIO();

                vk::RenderingAttachmentInfo color_attachment = {
                    .image_view_id = info.swap_chain_view,
                    .image_layout = vk::ImageLayout::ColorAttachment,
                    .load_op = vk::AttachmentLoadOp::Load,
                    .store_op = vk::AttachmentStoreOp::Store,
                };

                ImDrawData *draw_data = ImGui::GetDrawData();
                u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
                u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
                if (!draw_data || vertex_size_bytes == 0) {
                    return;
                }
            },
        });
    }

    bool on_surface_lost() override { return true; }
};

}  // namespace lr
