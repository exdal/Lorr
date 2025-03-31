module visbuffer;

import std;
import scene;

public struct VisBuffer {
    constexpr static u32 MESHLET_ID_BITS = 24u;
    constexpr static u32 MESHLET_ID_MASK = (1u << MESHLET_ID_BITS) - 1u;
    // We can support up to 256 triangles per meshlet.
    // Do not ever go higher than that.
    constexpr static u32 MESHLET_PRIMITIVE_BITS = 8u;
    constexpr static u32 MESHLET_PRIMITIVE_MASK = (1u << MESHLET_PRIMITIVE_BITS) - 1u;

    public u32 meshlet_index;
    public u32 triangle_index;

    [[mutating]]
    public __init(u32 meshlet_index, u32 triangle_index) {
        this.meshlet_index = meshlet_index;
        this.triangle_index = triangle_index;
    }

    [[mutating]]
    public __init(u32 data) {
        this.meshlet_index = (data >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
        this.triangle_index = data & MESHLET_PRIMITIVE_MASK;
    }

    public func encode() -> u32 {
        return (this.meshlet_index << MESHLET_PRIMITIVE_BITS) | (this.triangle_index & MESHLET_PRIMITIVE_MASK);
    }
};

