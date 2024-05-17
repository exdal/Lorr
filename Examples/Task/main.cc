#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/Task/TaskGraph.hh"

#include "OS/Window.hh"

using namespace lr;
using namespace lr::graphics;

struct BasicTask {
    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();

        RenderingAttachmentInfo attachment = tc.as_color_attachment(uses.attachment, ColorClearValue(0.1f, 0.1f, 0.1f, 1.0f));
        RenderingBeginInfo render_info = {
            .render_area = { 0, 0, 1280, 780 },
            .color_attachments = { &attachment, 1 },
        };
        cmd_list.begin_rendering(render_info);
        cmd_list.end_rendering();
    }
};

i32 main(i32 argc, c8 **argv)
{
    Log::init(argc, argv);

    ShaderCompiler::init();
    os::Window window;
    Instance instance;
    Device device;
    SwapChain swap_chain;
    std::array<ImageID, 3> images = {};
    std::array<ImageViewID, 3> image_views = {};

    auto create_swap_chain = [&](u32 width, u32 height) {
        if (swap_chain) {
            device.delete_swap_chains({ &swap_chain, 1 });
            device.delete_image_views(image_views);
            device.delete_images(images);
        }

        device.create_swap_chain(swap_chain, { window.get_surface(instance.m_handle), { width, height } });
        device.get_swapchain_images(swap_chain, images);
        for (u32 i = 0; i < images.size(); i++) {
            ImageViewInfo image_view_info = { .image_id = images[i] };
            auto [image_view_id, r_image_view] = device.create_image_view(image_view_info);
            image_views[i] = image_view_id;
        }
    };

    instance.init({});
    window.init({ .title = "Task Graph", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init(&instance.m_handle);
    create_swap_chain(window.m_width, window.m_height);

    TaskGraph task_graph;
    task_graph.init({ .device = &device });
    TaskImageID swap_chain_image = task_graph.add_image({ .image_id = images[0], .image_view_id = image_views[0] });

    task_graph.add_task<BasicTask>({
        .uses = { .attachment = swap_chain_image },
    });
    task_graph.present(swap_chain_image);

    while (!window.should_close()) {
        auto [acquire_sema, present_sema] = swap_chain.binary_semas();
        auto [acquired_index, acq_result] = device.acquire_next_image(swap_chain, acquire_sema);

        ImageID image_id = images[acquired_index];
        ImageViewID image_view_id = image_views[acquired_index];
        Semaphore &frame_sema = swap_chain.frame_sema();
        Semaphore &garbage_collector_sema = device.m_garbage_timeline_sema;
        CommandQueue &present_queue = device.get_queue(CommandType::Graphics);

        SemaphoreSubmitInfo wait_semas[] = { { acquire_sema, 0, PipelineStage::TopOfPipe } };
        SemaphoreSubmitInfo signal_semas[] = {
            { present_sema, 0, PipelineStage::BottomOfPipe },
            { frame_sema, frame_sema.advance(), PipelineStage::AllCommands },
            { garbage_collector_sema, garbage_collector_sema.advance(), PipelineStage::AllCommands },
        };

        task_graph.set_image(swap_chain_image, { .image_id = image_id, .image_view_id = image_view_id });
        task_graph.execute({ .image_index = acquired_index, .wait_semas = wait_semas, .signal_semas = signal_semas });

        present_queue.present(swap_chain, present_sema, acquired_index);
        device.collect_garbage();
    }

    return 1;
}
