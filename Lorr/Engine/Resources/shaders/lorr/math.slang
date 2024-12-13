implementing "../lorr";

/// CONSTANTS ///
// To sky unit, in our case meters to KM
public const static float CAMERA_SCALE_UNIT    = 0.01;
public const static float PLANET_RADIUS_OFFSET = 0.001;

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
    float denom = 1.0 + g2 + 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

///
/// GENERAL MATH ///
///

public float safe_sqrt(float x) {
    return sqrt(max(0, x));
}

public Optional<float> ray_sphere_intersect_nearest(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;

    // No intersection state.
    if (delta < 0.0 || a == 0.0) {
        return none;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);

    // Intersection on negative direction, no suitable for ray.
    if (sol1 < 0.0) { // When sol1 < 0.0, sol0 < 0.0 too.
        return none;
    }

    // Maybe exist one positive intersection.
    if (sol0 < 0.0) {
        return max(0.0, sol1);
    }

    // Two positive intersection, return nearest one.
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

    // Intersection on negative direction, no suitable for ray.
    if (sol1 <= 0.0 || sol0 <= 0.0) {
        return none;
    }

    // Two positive intersection, return nearest one.
    return float2(sol0, sol1);
}

// HLSL fmod is not same as GLSL mod!!!!!!!
public float3 mod(float3 x, float3 y) {
  return x - y * floor(x / y);
}

}
