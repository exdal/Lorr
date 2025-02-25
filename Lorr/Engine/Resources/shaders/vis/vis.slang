module vis;

import "../lorr";

public constexpr static u32 MESHLET_ID_BITS = 24u;
public constexpr static u32 MESHLET_ID_MASK = (1u << MESHLET_ID_BITS) - 1u;
// We can support up to 256 triangles per meshlet.
// Do not ever go higher than that.
public constexpr static u32 MESHLET_PRIMITIVE_BITS = 8u;
public constexpr static u32 MESHLET_PRIMITIVE_MASK = (1u << MESHLET_PRIMITIVE_BITS) - 1u;

