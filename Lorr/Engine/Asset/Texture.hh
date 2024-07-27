#pragma once

#include "Engine/Graphics/zfwd.hh"

namespace lr {
struct Texture {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    SamplerID sampler_id = SamplerID::Invalid;
};
}  // namespace lr
