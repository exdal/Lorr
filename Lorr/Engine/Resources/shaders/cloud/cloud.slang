module cloud;

import "../lorr";
import "../atmos/atmos";

public float remap(float orig, float old_min, float old_max, float new_min, float new_max) {
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

// WARN: These hashes are extremely fragile. Replacing one of them would collapse all noise functions.
public float hash13(float3 p) {
    const static uint3 VPRIME = uint3(3480082861U, 2420690917U, 2149110343U);
    const static uint FPRIME = 1317666547U;
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * UIF;
}

public float3 hash33(float3 p) {
    const static uint UI0 = 1597334673U;
    const static uint UI1 = 3812015801U;
    const static uint2 UI2 = uint2(UI0, UI1);
    const static uint3 UI3 = uint3(UI0, UI1, 2798796415U);
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z) * UI3;
    return float3(q) * UIF;
}


float3 get_cloud_layer_boundaries(in Atmosphere atmosphere, in Clouds clouds, float3 eye_pos, float3 eye_dir) {
    float planet_radius = atmosphere.planet_radius;
    float2 cloud_layer_boundaries = clouds.bounds + float2(planet_radius);
    float eye_altitude = length(eye_pos);

    if (eye_altitude < cloud_layer_boundaries.x) {
        // Camera is under cloud layer
        float t_min = lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.x);
        float t_max = lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.y);
        float t_earth = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, planet_radius).hasValue ? 1.0 : 0.0;

        return float3(t_min, t_max, t_earth);
    } else if (eye_altitude > cloud_layer_boundaries.y) {
        // Camera is above cloud layer
        const let tv0 = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.y);
        const let tv1 = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.x);
        
        if (tv1.hasValue) {
            return float3(tv0.value.x, tv1.value.x, tv0.hasValue ? 0.0 : 1.0);
        } else {
            return float3(tv0.value.x, tv1.value.y, tv0.hasValue ? 0.0 : 1.0);
        }
    } else {
        // Camera is inside cloud layer
        const let x = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, cloud_layer_boundaries.x);
        if (x.hasValue) {
            return float3(0.0, x.value, 0.0);
        } else {
            return float3(0.0, lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.y), 0.0);
        }
    }
}

