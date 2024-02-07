[[vk::binding(0, 0)]] SamplerState  u_samplers[];
[[vk::binding(0, 1)]] Texture2D     u_textures[];
[[vk::binding(0, 2)]] Texture2D<>   u_rwtextures[];
[[vk::binding(0, 3)]] RWStructuredBuffer<uint64_t> u_buffer_address;

template<uint32_t CountT>
struct Descriptors
{
    uint32_t indexes[CountT / 2];

    uint32_t get_id(uint32_t id)
    {
        if (id & 1)
            return indexes[id] >> 16;

        return indexes[id] & 0xFFFF;
    }

    Texture2D get_texture(uint32_t id)
    {
        return u_textures[get_id(id)];
    }

    SamplerState get_sampler(uint32_t id)
    {
        return u_samplers[get_id(id)];
    }
};
