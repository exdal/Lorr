#pragma once

#include "PipelineManager.hh"

#include "Graphics/CommandList.hh"

namespace lr::Renderer
{
struct Task;
struct TaskContext;
struct TaskGraph;
// Special type of command buffer wrapper just for task graph
struct TaskCommandList
{
    // Memory barrier config
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;
    // Descriptor buffer config

    TaskCommandList(VkCommandBuffer command_buffer, TaskGraph &task_graph);
    ~TaskCommandList();
    void set_context(TaskContext *context);

    void insert_memory_barrier(Graphics::MemoryBarrier &barrier);
    void insert_image_barrier(Graphics::ImageBarrier &barrier);
    void flush_barriers();

    void begin_rendering(const Graphics::RenderingBeginDesc &begin_desc);
    void end_rendering();
    TaskCommandList &set_dynamic_state(Graphics::DynamicState states);
    TaskCommandList &set_pipeline(eastl::string_view pipeline_name);
    TaskCommandList &set_viewport(u32 id, const glm::vec4 &viewport, const glm::vec2 &depth = { 1.0, 0.01 });
    TaskCommandList &set_scissors(u32 id, const glm::uvec4 &rect);
    TaskCommandList &set_blend_state(u32 id, const Graphics::ColorBlendAttachment &state);
    TaskCommandList &set_blend_state_all(const Graphics::ColorBlendAttachment &state);
    void draw(u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0);
    void draw_indexed(u32 index_count, u32 first_index = 0, u32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0);
    void dispatch(u32 group_x, u32 group_y, u32 group_z);

private:
    void compile_and_bind_pipeline();

    fixed_vector<Graphics::MemoryBarrier, kMaxMemoryBarriers> m_memory_barriers = {};
    fixed_vector<Graphics::ImageBarrier, kMaxImageBarriers> m_image_barriers = {};
    Graphics::DynamicState m_dynamic_states = {};
    eastl::string m_bound_pipeline_name = {};
    Graphics::Pipeline *m_pipeline = nullptr;
    TaskContext *m_context = nullptr;
    TaskGraph &m_task_graph;

    VkCommandBuffer m_handle = nullptr;
};

}  // namespace lr::Renderer