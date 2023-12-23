[[vk::binding(0, 0)]] RWStructuredBuffer<uint32_t> u_descriptor_indices;
[[vk::binding(0, 1)]] RWStructuredBuffer<uint64_t> u_buffer_address;
[[vk::binding(0, 2)]] SamplerState  u_samplers[];
[[vk::binding(0, 3)]] Texture2D     u_textures[];

struct Descriptors
{
    uint32_t first;

    Texture2D get_texture(uint id)
    {
        return u_textures[u_descriptor_indices[first + id]];
    }
    SamplerState get_sampler(uint id)
    {
        return u_samplers[u_descriptor_indices[first + id]];
    }
};