#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"

#include "Memory/Stack.hh"

#include "OS/Window.hh"

#define EXAMPLE_DIR (LR_PROJECT_DIR "/Examples/BareBones")

using namespace lr;
using namespace lr::graphics;

struct App {
    ShaderID triangle_vs;
    ShaderID triangle_fs;
    PipelineID triangle_pipeline;
} app;

Result<std::string, os::FileResult> load_shader_file(std::string_view path)
{
    std::string code;
    auto [file, result] = os::open_file(path, os::FileAccess::Read);
    if (!result) {
        LR_LOG_ERROR("Failed to load shader file {}!", path);
        return result;
    }

    code.resize(os::file_size(file));
    os::read_file(file, code.data(), { 0, ~0u });
    os::close_file(file);

    return std::move(code);
}

bool load_shaders(Device &device)
{
    ZoneScoped;

    VirtualFileInfo vfile_infos[] = {
        { LR_SHADER_STD_FILE_PATH, "lorr" },
    };
    VirtualDir vdir(std::span{ vfile_infos });

    ShaderPreprocessorMacroInfo macros[] = {
        { "LR_EDITOR", "1" },
#if LR_DEBUG
        { "LR_DEBUG", "1" },
#endif
    };

    ShaderCompileInfo info = {
        .virtual_env = { &vdir, 1 },
        .definitions = macros,
    };

    {
        memory::ScopedStack stack;
        std::string_view path = stack.format("{}/triangle.slang", EXAMPLE_DIR);
        auto [code, result] = load_shader_file(path);
        info.code = code;
        info.real_path = path;
        info.entry_point = "vs_main";
        auto [vs_data, vs_result] = ShaderCompiler::compile(info);
        info.entry_point = "fs_main";
        auto [fs_data, fs_result] = ShaderCompiler::compile(info);

        app.triangle_vs = device.create_shader(ShaderStageFlag::Vertex, vs_data);
        app.triangle_fs = device.create_shader(ShaderStageFlag::Pixel, fs_data);
    }

    return true;
}

int main(int argc, char *argv[])
{
    lr::Log::init(argc, argv);

    ShaderCompiler::init();

    os::Window window;
    Instance instance;
    Device device;
    Unique<SwapChain> swap_chain(&device);
    Unique<std::array<ImageID, 3>> images(&device);
    Unique<std::array<ImageViewID, 3>> image_views(&device);
    Unique<std::array<CommandAllocator, 3>> command_allocators(&device);

    instance.init({});
    window.init({ .title = "BareBones", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init(&instance.m_handle);

    swap_chain = Unique<SwapChain>(&device);
    device.create_swap_chain(*swap_chain, { window.get_surface(instance.m_handle), { window.m_width, window.m_height } });

    device.get_swapchain_images(*swap_chain, *images);
    for (u32 i = 0; i < images->size(); i++) {
        ImageViewInfo image_view_info = { .image_id = images->at(i) };
        auto [image_view_id, r_image_view] = device.create_image_view(image_view_info);
        image_views->at(i) = image_view_id;
    }

    device.create_command_allocators(*command_allocators, { CommandType::Graphics });

    load_shaders(device);

    {
        std::array shader_ids = { app.triangle_vs, app.triangle_fs };
        Viewport viewport = { 0.0f, 0.0f, f32(window.m_width), f32(window.m_height) };
        Rect2D scissors = { 0, 0, window.m_width, window.m_height };
        PipelineColorBlendAttachment blend_attachment = {};

        GraphicsPipelineInfo triangle_pipeline_info = {
            .color_attachment_formats = { &swap_chain->m_format, 1 },
            .viewports = { &viewport, 1 },
            .scissors = { &scissors, 1 },
            .shader_ids = shader_ids,
            .blend_attachments = { &blend_attachment, 1 },
        };
        app.triangle_pipeline = device.create_graphics_pipeline(triangle_pipeline_info);
    }

    while (!window.should_close()) {
        memory::ScopedStack stack;

        auto [acquire_sema, present_sema] = swap_chain->binary_semas();
        auto [acq_index, acq_result] = device.acquire_next_image(*swap_chain, acquire_sema);
        Semaphore &frame_sema = swap_chain->frame_sema();
        CommandQueue &queue = device.get_queue(CommandType::Graphics);
        ImageID image_id = images->at(acq_index);
        ImageViewID image_view_id = image_views->at(acq_index);
        CommandAllocator &command_allocator = command_allocators->at(acq_index);

        device.reset_command_allocator(command_allocator);
        {
            Unique<CommandList> command_list(&device);
            device.create_command_lists({ &*command_list, 1 }, command_allocator);
            command_list.set_name(stack.format("Transient list = {}", acq_index));
            device.begin_command_list(*command_list);

            ImageBarrier attachment_barrier = {
                .src_access = PipelineAccess::TopOfPipe,
                .dst_access = PipelineAccess::ColorAttachmentWrite,
                .old_layout = ImageLayout::Undefined,
                .new_layout = ImageLayout::ColorAttachment,
                .image_id = image_id,
            };
            command_list->set_barriers({}, { &attachment_barrier, 1 });

            const glm::vec3 half = glm::vec3(0.5f);
            glm::vec3 color = half + half * glm::cos(glm::vec3(float(glfwGetTime())) + glm::vec3(0, 2, 4));
            RenderingAttachmentInfo attachment = {
                .image_view_id = image_view_id,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { ColorClearValue(color.x, color.y, color.z, 1.0f) },
            };
            RenderingBeginInfo render_info = {
                .render_area = { 0, 0, window.m_width, window.m_height },
                .color_attachments = { &attachment, 1 },
            };
            command_list->begin_rendering(render_info);
            command_list->set_pipeline(app.triangle_pipeline);
            command_list->draw(3);
            command_list->end_rendering();

            ImageBarrier present_barrier = {
                .src_access = PipelineAccess::ColorAttachmentWrite,
                .dst_access = PipelineAccess::BottomOfPipe,
                .old_layout = ImageLayout::ColorAttachment,
                .new_layout = ImageLayout::Present,
                .image_id = image_id,
            };
            command_list->set_barriers({}, { &present_barrier, 1 });
            device.end_command_list(*command_list);

            SemaphoreSubmitInfo wait_semas[] = { { acquire_sema, 0, PipelineStage::TopOfPipe } };
            CommandListSubmitInfo command_list_infos[] = { { *command_list } };
            SemaphoreSubmitInfo signal_semas[] = {
                { present_sema, 0, PipelineStage::BottomOfPipe },
                { frame_sema, frame_sema.advance(), PipelineStage::AllCommands },
                { queue.semaphore(), queue.semaphore().advance(), PipelineStage::AllCommands },
            };

            QueueSubmitInfo submit_info = {
                .wait_sema_count = static_cast<u32>(count_of(wait_semas)),
                .wait_sema_infos = wait_semas,
                .command_list_count = static_cast<u32>(count_of(command_list_infos)),
                .command_list_infos = command_list_infos,
                .signal_sema_count = static_cast<u32>(count_of(signal_semas)),
                .signal_sema_infos = signal_semas,
            };
            queue.submit(submit_info);
        }

        queue.present(*swap_chain, present_sema, acq_index);
        device.collect_garbage();
        window.poll();
    }
    return 0;
}
