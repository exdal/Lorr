#include "ExampleBase.hh"

struct BareBones : lr::ExampleBase {
    BareBones(i32 argc, char *argv[])
        : lr::ExampleBase(argc, argv)
    {
    }

    void update(CommandAllocator &command_allocator, ImageID image_id, ImageViewID image_view_id) override
    {
        Unique<CommandList> command_list(&m_device);
        m_device.create_command_lists({ &*command_list, 1 }, command_allocator);
        command_list.set_name(fmt::format("Transient list = {}", (u32)image_id));
        m_device.begin_command_list(*command_list);

        CommandBatcher batcher(command_list);
        auto attachment_barrier = make_barrier(
            image_id, ImageLayout::Undefined, TaskAccess::TopOfPipe, ImageLayout::ColorAttachment, TaskAccess::ColorAttachmentWrite);
        batcher.insert_image_barrier(attachment_barrier);
        batcher.flush_barriers();

        RenderingAttachmentInfo attachment = {
            .image_view = *m_device.get_image_view(image_view_id),
            .image_layout = ImageLayout::ColorAttachment,
            .load_op = AttachmentLoadOp::Clear,
            .store_op = AttachmentStoreOp::Store,
            .color_clear = { 1.0f, 0.0f, 0.0f, 1.0f },
        };
        RenderingBeginInfo render_info = {
            .render_area = { 0, 0, m_window.m_width, m_window.m_height },
            .layer_count = 1,
            .color_attachment_count = 1,
            .color_attachments = &attachment,
        };
        command_list->begin_rendering(render_info);
        command_list->end_rendering();

        auto present_barrier = make_barrier(
            image_id, ImageLayout::ColorAttachment, TaskAccess::ColorAttachmentWrite, ImageLayout::Present, TaskAccess::BottomOfPipe);
        batcher.insert_image_barrier(present_barrier);
        batcher.flush_barriers();
        m_device.end_command_list(*command_list);

        std::vector<SemaphoreSubmitInfo> wait_semas;
        std::vector<CommandListSubmitInfo> command_lists = { { *command_list } };
        std::vector<SemaphoreSubmitInfo> signal_semas;
        command_list.reset();
        submit(CommandType::Graphics, wait_semas, command_lists, signal_semas, true);
    }
};

int main(int argc, char *argv[])
{
    BareBones example(argc, argv);
    example.init();
    return 0;
}
