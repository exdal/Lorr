implementing "../lorr";

/// CONSTANTS ///
// To sky unit, in our case meters to KM
public const static float CAMERA_SCALE_UNIT     = 0.01;
public const static float INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
public const static float PLANET_RADIUS_OFFSET  = 0.001;

///
/// PAHASE FUNCTIONS ///
///

public namespace lr {
public constexpr float uniform_phase() {
    return 1.0 / (4.0 * PI);
}

public float rayleigh_phase(float altitude) {
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

public float cornette_shanks_phase(float g, float altitude) {
    const float g2 = g * g;
    const float scale = 3.0 / (8.0 * PI);

    float num = (1.0 - g2) * (1.0 + altitude * altitude);
    float denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);

    return scale * num / denom;
}

public float henyey_greenstein_phase(float g, float cos_theta) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

///
/// GENERAL MATH ///
///

public float safe_sqrt(float x) {
    return sqrt(max(0, x));
}

public float2 ray_sphere_intersect(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;

    if (delta < 0.0) {
        return -1.0;
    }

    float sqrt_delta = sqrt(delta);
    return (-b + float2(-1.0, 1.0) * sqrt_delta) / (2.0 * a);
}

public Optional<float> ray_sphere_intersect_nearest(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;

    if (delta < 0.0) {
        return none;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);

    if (sol0 < 0.0 && sol1 < 0.0) {
        return none;
    }

    if (sol0 < 0.0) {
        return max(0.0, sol1);
    } else if (sol1 < 0.0) {
        return max(0.0, sol0);
    }

    return max(0.0, min(sol0, sol1));
}

public float ray_sphere_intersect_inside(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);
    return sol1;
}

public Optional<float2> ray_sphere_intersect_outside(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;
    if (delta < 0.0 || a == 0.0) {
        return none;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);

    if (sol1 <= 0.0 || sol0 <= 0.0) {
        return none;
    }

    return float2(sol0, sol1);
}

// HLSL fmod is not same as GLSL mod!!!!!!!
public float3 mod(float3 x, float3 y) {
  return x - y * floor(x / y);
}

// Unit vectors octahedral encoding
// "Survey of Efficient Representations for Independent Unit Vectors"
// http://jcgt.org/published/0003/02/01
public float3 oct_to_vec3(float2 e) {
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    const float2 sign_not_zero = float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero;
    }

    return normalize(v);
}

public float2 vec3_to_oct(float3 v) {
    float2 p = float2(v.x, v.y) * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    const float2 sign_not_zero = float2((p.x >= 0.0) ? 1.0 : -1.0, (p.y >= 0.0) ? 1.0 : -1.0);
    return (v.z <= 0.0) ? ((1.0 - abs(float2(p.y, p.x))) * sign_not_zero) : p;
}

// Color spaces
public float3 color_sRGB_EOTF(float3 srgb_nonlinear) {
    bool3 cutoff = srgb_nonlinear < vec3(0.04045);
    vec3 higher = pow((srgb_nonlinear + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower = srgb_nonlinear / vec3(12.92);

    return lerp(higher, lower, float3(cutoff));
}

public float3 color_sRGB_OETF(float3 srgb_linear) {
    bool3 cutoff = srgb_linear < 0.0031308;
    float3 higher = 1.055 * pow(srgb_linear, 1.0 / 2.4) - 0.055;
    float3 lower = srgb_linear * 12.92;

    return lerp(higher, lower, float3(cutoff));
}

}
