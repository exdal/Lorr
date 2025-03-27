implementing "../lorr";

namespace lr {
public float MM_Hash2(vec2 v) {
    return fract(1e4 * sin(17.0 * v.x + 0.1 * v.y) * (0.1 + abs(sin(13.0 * v.y + v.x))));
}

public float MM_Hash3(vec3 v) {
    return MM_Hash2(vec2(MM_Hash2(v.xy), v.z));
}
}
