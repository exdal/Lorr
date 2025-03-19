module cloud;

import "../lorr";
import "../atmos/atmos";

public float remap(float orig, float old_min, float old_max, float new_min, float new_max) {
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

public float3 get_cloud_layer_boundaries(in Atmosphere atmosphere, in Clouds clouds, float3 eye_pos, float3 eye_dir) {
    float planet_radius = atmosphere.planet_radius;
    float2 cloud_layer_boundaries = clouds.bounds + float2(planet_radius);
    float eye_altitude = length(eye_pos);

    if (eye_altitude < cloud_layer_boundaries.x) {
        // Camera is under cloud layer
        const let x = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, cloud_layer_boundaries.x).value;
        const let y = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, cloud_layer_boundaries.y).value;
        const let planet = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, planet_radius).hasValue ? 1.0 : 0.0;
        return float3(x, y, planet);
    } else if (eye_altitude > cloud_layer_boundaries.y) {
        // Camera is above cloud layer
        const let x = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.y);
        if (!x.hasValue) {
            return float3(0.0, 0.0, 1.0);
        }

        const let y = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.x);
        if (y.hasValue) {
            return float3(x.value.x, y.value.x, 0.0);
        } else {
            return float3(x.value.x, x.value.y, 0.0);
        }
    } else {
        // Camera is inside cloud layer
        const let x = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, cloud_layer_boundaries.x);
        if (x.hasValue) {
            return float3(0.0, x.value, 0.0);
        } else {
            const let y = lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.y);
            return float3(0.0, y, 0.0);
        }
    }

    return float3(0.0, 0.0, 1.0);
}

