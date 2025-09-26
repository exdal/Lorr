module visbuffer;

import std;
import scene;

constexpr static u32 MESHLET_INSTANCE_ID_BITS = 24u;
constexpr static u32 MESHLET_INSTANCE_ID_MASK = (1u << MESHLET_INSTANCE_ID_BITS) - 1u;

public constexpr static u32 MESHLET_PRIMITIVE_BITS = 8u;
public constexpr static u32 MESHLET_PRIMITIVE_MASK = (1u << MESHLET_PRIMITIVE_BITS) - 1u;

public struct VisBufferData {
    public u32 meshlet_instance_index;
    public u32 triangle_index;

    [[mutating]]
    public __init(u32 meshlet_instance_index, u32 triangle_index) {
        this.meshlet_instance_index = meshlet_instance_index;
        this.triangle_index = triangle_index;
    }

    [[mutating]]
    public __init(u32 data) {
        this.meshlet_instance_index = (data >> MESHLET_PRIMITIVE_BITS) & MESHLET_INSTANCE_ID_MASK;
        this.triangle_index = data & MESHLET_PRIMITIVE_MASK;
    }

    public func encode() -> u32 {
        return (this.meshlet_instance_index << MESHLET_PRIMITIVE_BITS) | (this.triangle_index & MESHLET_PRIMITIVE_MASK);
    }
};