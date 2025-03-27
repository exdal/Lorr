implementing std;

import glm;

public namespace std {
// Unit vectors octahedral encoding
// "Survey of Efficient Representations for Independent Unit Vectors"
// http://jcgt.org/published/0003/02/01
public func oct_to_vec3(glm::vec2 e) -> glm::vec3 {
    glm::vec3 v = glm::vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    const let sign_not_zero = glm::vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero;
    }

    return normalize(v);
}

public func vec3_to_oct(glm::vec3 v) -> glm::vec2 {
    const let p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    const let sign_not_zero = glm::vec2((p.x >= 0.0) ? 1.0 : -1.0, (p.y >= 0.0) ? 1.0 : -1.0);
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * sign_not_zero) : p;
}

// Color spaces
public func color_sRGB_EOTF(glm::vec3 srgb_nonlinear) -> glm::vec3 {
    bool3 cutoff = srgb_nonlinear < 0.04045;
    const let higher = pow((srgb_nonlinear + 0.055) / 1.055, 2.4);
    const let lower = srgb_nonlinear / 12.92;

    return lerp(higher, lower, reinterpret<glm::vec3>(cutoff));
}

public func color_sRGB_OETF(glm::vec3 srgb_linear) -> glm::vec3 {
    bool3 cutoff = srgb_linear < 0.0031308;
    const let higher = 1.055 * pow(srgb_linear, 1.0 / 2.4) - 0.055;
    const let lower = srgb_linear * 12.92;

    return lerp(higher, lower, reinterpret<glm::vec3>(cutoff));
}
}