
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

    Texture2D get_image(uint32_t id)
    {
        return u_images[get_id(id)];
    }

    SamplerState get_sampler(uint32_t id)
    {
        return u_samplers[get_id(id)];
    }
};
