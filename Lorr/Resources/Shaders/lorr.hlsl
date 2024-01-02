[[vk::binding(0, 0)]] SamplerState  u_samplers[];
[[vk::binding(0, 1)]] Texture2D     u_textures[];
[[vk::binding(0, 2)]] Texture2D<>   u_rwtextures[];
[[vk::binding(0, 3)]] RWStructuredBuffer<uint32_t> u_descriptor_indexes;
[[vk::binding(0, 4)]] RWStructuredBuffer<uint64_t> u_buffer_address;

struct Descriptors
{
    uint32_t first;

    Texture2D get_texture(uint32_t id)
    {
        return u_textures[u_descriptor_indexes[first + id]];
    }

    SamplerState get_sampler(uint32_t id)
    {
        return u_samplers[u_descriptor_indexes[first + id]];
    }
};