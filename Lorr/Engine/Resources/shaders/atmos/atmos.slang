module atmos;

import "../lorr";

public float fromSubUvsToUnit(float u, float resolution) {
    return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}

public float fromUnitToSubUvs(float u, float resolution) {
    return (u + 0.5f / resolution) * (resolution / (resolution + 1.0));
}

public float2 get_transmittance_lut_uv(in Atmosphere atmos, float2 p) {
    float h = lr::safe_sqrt(
        atmos.atmos_radius * atmos.atmos_radius -
        atmos.planet_radius * atmos.planet_radius);
    float rho = lr::safe_sqrt(p.x * p.x - atmos.planet_radius * atmos.planet_radius);
    float discriminant = p.x * p.x * (p.y * p.y - 1.0) +
        atmos.atmos_radius * atmos.atmos_radius;
    float d = max(0.0, -p.x *  p.y + lr::safe_sqrt(discriminant));
    float d_min = atmos.atmos_radius - p.x;
    float d_max = rho + h;
    float mu = (d - d_min) / (d_max - d_min);
    float r = rho / h;
    return float2(mu, r);
}

public float2 get_multiscattering_lut_uv(in Atmosphere atmos, float altitude, float cos_theta) {
    float2 uv = clamp(float2(
        cos_theta * 0.5 + 0.5,
        altitude / (atmos.atmos_radius - atmos.planet_radius)),
        0.0, 1.0
    );
    return float2(
        fromUnitToSubUvs(uv.x, f32(atmos.multiscattering_lut_size.x)),
        fromUnitToSubUvs(uv.y, f32(atmos.multiscattering_lut_size.y)));
}

public float2 get_sky_view_lut_uv(in Atmosphere atmos, bool intersect_planet, float view_zenith_cos, float light_view_cos, float altitude) {
    float2 uv = 0.0;
    float horizon = sqrt(altitude * altitude - atmos.planet_radius * atmos.planet_radius);
    float beta = acos(horizon / altitude);
    float zenith_horizon_angle = PI - beta;
        // Non-linear parametization
    if (!intersect_planet) {
        float coord = acos(view_zenith_cos) / zenith_horizon_angle;
        coord = 1.0 - coord;
        coord = sqrt(coord);
        coord = 1.0 - coord;
        uv.y = 0.5 * coord;
    } else {
        float coord = (acos(view_zenith_cos) - zenith_horizon_angle) / beta;
        coord = sqrt(coord);
        uv.y = 0.5 * coord + 0.5;  // Non-linear parametization
    }

    uv.x = sqrt(-light_view_cos * 0.5 + 0.5);

    return uv;
}

public bool move_to_top_atmosphere(inout float3 pos, float3 dir, float atmos_radius) {
    float h = length(pos);
    if (h > atmos_radius) {
        const let top_intersection = lr::ray_sphere_intersect_nearest(pos, dir, atmos_radius);
        if (top_intersection.hasValue) {
            float3 up_vec = pos / h;
            float3 up_offset = up_vec * -PLANET_RADIUS_OFFSET;
            pos = pos + dir * top_intersection.value + up_offset;
        } else {
            return false;
        }
    }
    return true;
}

public struct MediumScattering {
    public float3 rayleigh_scattering;
    public float3 rayleigh_extinction;

    public float3 mie_scattering;
    public float3 mie_extinction;

    public float3 ozone_absorption;
    public float3 ozone_extinction;

    public float3 scattering_sum;
    public float3 extinction_sum;

    [ForceInline]
    public __init(in Atmosphere atmos, float altitude) {
        const float rayleigh_density  = exp(-altitude / atmos.rayleigh_density);
        const float mie_density = exp(-altitude / atmos.mie_density);
        const float ozone_density = max(0.0, 1.0 - abs(altitude - atmos.ozone_height) / atmos.ozone_thickness);

        this.rayleigh_scattering = atmos.rayleigh_scatter * rayleigh_density;
        this.rayleigh_extinction = this.rayleigh_scattering;  // Rayleigh scattering doesn't have absorption behavior

        this.mie_scattering = atmos.mie_scatter * mie_density;
        this.mie_extinction = atmos.mie_extinction * mie_density;  // Mie scattering doesn't have absorption behavior

        this.ozone_absorption = atmos.ozone_absorption * ozone_density;
        this.ozone_extinction = this.ozone_absorption;

        this.scattering_sum = this.rayleigh_scattering + this.mie_scattering;
        this.extinction_sum = this.rayleigh_extinction + this.mie_extinction + this.ozone_extinction;
    }
};

public struct AtmosphereLuminance {
    public float3 luminance;
    public float3 multiscattering_as_1;
    public float3 transmittance;

    public __init() {
        this.luminance = float3(0.0);
        this.multiscattering_as_1 = float3(0.0);
        this.transmittance = float3(1.0);
    }
};

public struct AtmosphereSampling {
    public bool variable_sample_count;
    public float initial_sample_count;
    public float min_sample_count;
    public float max_sample_count;
    public float inv_distance_to_sample_count_max;

    public __init() {
        this.variable_sample_count = false;
        this.initial_sample_count = 0.0;
        this.min_sample_count = 0.0;
        this.max_sample_count = 0.0;
        this.inv_distance_to_sample_count_max = 0.01;
    }
};

public struct AtmosphereIntegrateInfo {
    public float3 eye_pos;
    public float3 eye_dir;
    public float3 sun_dir;
    public float sun_intensity;
    public float max_integration_length;
    public AtmosphereSampling sampling;
    public bool eval_mie_rayleigh_phase;
    public bool eval_planet_luminance;

    public Texture2D<float4> transmittance_image;
#ifndef ATMOS_NO_EVAL_MS
    public Texture2D<float4> multiscattering_image;
#endif

    public __init() {
        this.eye_pos = {};
        this.eye_dir = {};
        this.sun_dir = {};
        this.sun_intensity = 1.0;
        this.max_integration_length = 9000000.0;
        this.sampling = {};
        this.eval_mie_rayleigh_phase = true;
        this.eval_planet_luminance = false;
        this.transmittance_image = {};
#ifndef ATMOS_NO_EVAL_MS
        this.multiscattering_image = {};
#endif
    }
};

public func integrate_single_scattered_luminance(
    in SamplerState lut_sampler,
    in Atmosphere atmos,
    in AtmosphereIntegrateInfo info) -> AtmosphereLuminance
{
    AtmosphereLuminance result = {};

    const let atmos_intersection = lr::ray_sphere_intersect_nearest(info.eye_pos, info.eye_dir, atmos.atmos_radius);
    const let planet_intersection = lr::ray_sphere_intersect_nearest(info.eye_pos, info.eye_dir, atmos.planet_radius);
    float integration_length = 0.0;
    if (!atmos_intersection.hasValue && !planet_intersection.hasValue) {
        // No intersection
        return {};
    } else if (atmos_intersection.hasValue && !planet_intersection.hasValue) {
        // Atmosphere only intersection
        integration_length = atmos_intersection.value;
    } else if (!atmos_intersection.hasValue && planet_intersection.hasValue) {
        // Planet only intersection
        integration_length = planet_intersection.value;
    } else {
        // Ray intersects both atmosphere and planet, select the closest intersection
        integration_length = min(atmos_intersection.value, planet_intersection.value);
    }

    integration_length = min(integration_length, info.max_integration_length);
    float sample_count = info.sampling.initial_sample_count;
    float sample_count_floor = info.sampling.initial_sample_count;
    float max_integration_length_floor = integration_length;
    if (info.sampling.variable_sample_count) {
        sample_count = lerp(info.sampling.min_sample_count, info.sampling.max_sample_count,
                            saturate(integration_length * info.sampling.inv_distance_to_sample_count_max));
        sample_count_floor = floor(sample_count);
        max_integration_length_floor = integration_length * sample_count_floor / sample_count;
    }

    float cos_theta = dot(info.sun_dir, info.eye_dir);
    float rayleigh_phase = lr::rayleigh_phase(cos_theta);
    float mie_phase = lr::henyey_greenstein_phase(atmos.mie_asymmetry, cos_theta);

    float step = 0.0;
    float delta_step = integration_length / sample_count;
    for (float i = 0; i < sample_count; i += 1.0) {
        if (info.sampling.variable_sample_count) {
            float cur_step  = (i + 0.0) / sample_count_floor;
            float next_step = (i + 1.0) / sample_count_floor;
            cur_step *= cur_step;
            next_step *= next_step;

            cur_step *= max_integration_length_floor;
            next_step = next_step > 1.0 ? integration_length : next_step * max_integration_length_floor;
            delta_step = next_step - cur_step;
            step = cur_step + delta_step * 0.3;
        } else {
            step = integration_length * (i + 0.3) / sample_count;
        }

        float3 step_pos = info.eye_pos + step * info.eye_dir;
        float h = length(step_pos);
        float altitude = h - atmos.planet_radius;
        MediumScattering medium_info = MediumScattering(atmos, altitude);

        float3 up_vec = normalize(step_pos);
        float3 up_vec_scaled = PLANET_RADIUS_OFFSET * up_vec;
        float earth_shadow = lr::ray_sphere_intersect_nearest(step_pos - up_vec_scaled, info.sun_dir, atmos.planet_radius).hasValue ? 0.0 : 1.0;
        f32 sun_theta = dot(info.sun_dir, up_vec);

        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, sun_theta));
        float3 sun_transmittance = info.transmittance_image.Sample(lut_sampler, transmittance_uv).rgb;

        float3 MS = float3(0.0);
#ifndef ATMOS_NO_EVAL_MS
        float2 multiscatter_uv = get_multiscattering_lut_uv(atmos, altitude, sun_theta);
        MS = info.multiscattering_image.Sample(lut_sampler, multiscatter_uv).rgb;
#endif
        float3 scattering_phase = float3(0.0);
        if (info.eval_mie_rayleigh_phase) {
            scattering_phase = medium_info.mie_scattering * mie_phase + medium_info.rayleigh_scattering * rayleigh_phase;
        } else {
            scattering_phase = medium_info.scattering_sum * lr::uniform_phase();
        }

        float3 sun_luminance = earth_shadow * sun_transmittance * scattering_phase + (MS * medium_info.scattering_sum);
        float3 step_transmittance = exp(-delta_step * medium_info.extinction_sum);

        float3 integral = (sun_luminance - sun_luminance * step_transmittance) / medium_info.extinction_sum;
        float3 ms_integral = (medium_info.scattering_sum - medium_info.scattering_sum * step_transmittance) / medium_info.extinction_sum;

        result.luminance += info.sun_intensity * (integral * result.transmittance);
        result.multiscattering_as_1 += ms_integral * result.transmittance;
        result.transmittance *= step_transmittance;
    }

    // Accumulate light bouncing off planet's ground
    if (info.eval_planet_luminance && planet_intersection.hasValue && integration_length == planet_intersection.value) {
        float3 planet_pos = info.eye_pos + integration_length * info.eye_dir;
        float h = length(planet_pos);
        float3 up_vec = planet_pos / h;
        float sun_theta = dot(info.sun_dir, up_vec);
        float NoL = saturate(dot(normalize(info.sun_dir), normalize(up_vec)));

        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, sun_theta));
        float3 sun_transmittance = info.transmittance_image.Sample(lut_sampler, transmittance_uv).rgb;

        result.luminance += info.sun_intensity * (sun_transmittance * result.transmittance * NoL * atmos.terrain_albedo / PI);
    }

    return result;
}

