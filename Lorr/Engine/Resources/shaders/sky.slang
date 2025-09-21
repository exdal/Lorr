module sky;

import gpu;
import std;
import scene;

public func from_sub_uvs_to_unit(f32x2 uv, f32x2 res) -> f32x2 {
    return (uv - 0.5 / res) * (res / (res - 1.0));
}

public func from_unit_to_sub_uvs(f32x2 uv, f32x2 res) -> f32x2 {
    return (uv + 0.5 / res) * (res / (res + 1.0));
}

public func transmittance_params_to_lut_uv(f32 atmos_radius, f32 planet_radius, f32x2 p) -> f32x2 {
    f32 h = std::safe_sqrt(atmos_radius * atmos_radius - planet_radius * planet_radius);
    f32 rho = std::safe_sqrt(p.x * p.x - planet_radius * planet_radius);
    f32 discriminant = p.x * p.x * (p.y * p.y - 1.0) + atmos_radius * atmos_radius;
    f32 d = max(0.0, -p.x * p.y + std::safe_sqrt(discriminant));
    f32 d_min = atmos_radius - p.x;
    f32 d_max = rho + h;
    f32 mu = (d - d_min) / (d_max - d_min);
    f32 r = rho / h;

    return f32x2(mu, r);
}

public func multiscattering_params_to_lut_uv(
    f32 atmos_radius,
    f32 planet_radius,
    i32x2 multiscattering_lut_size,
    f32 altitude,
    f32 cos_theta
) -> f32x2 {
    f32x2 uv = clamp(f32x2(cos_theta * 0.5 + 0.5, altitude / (atmos_radius - planet_radius)), 0.0, 1.0);
    return from_unit_to_sub_uvs(uv, f32x2(multiscattering_lut_size));
}

public func sky_view_params_to_lut_uv(
    f32 atmos_radius,
    f32 planet_radius,
    i32x2 sky_view_lut_size,
    bool intersect_planet,
    f32 altitude,
    f32 view_zenith_cos_angle,
    f32 light_view_angle
) -> f32x2 {
    var uv: f32x2;
    if (altitude < atmos_radius) {
        let beta = asin(planet_radius / altitude);
        let zenith_horizon_angle = PI - beta;
        if (!intersect_planet) {
            var coord = view_zenith_cos_angle / zenith_horizon_angle;
            coord = (1.0 - std::safe_sqrt(1.0 - coord)) * 0.5;
            uv.y = coord;
        } else {
            var coord = (view_zenith_cos_angle - zenith_horizon_angle) / beta;
            coord = (std::safe_sqrt(coord) + 1.0) * 0.5;
            uv.y = coord;
        }
    } else {
        let beta = asin(atmos_radius / altitude);
        let zenith_horizon_angle = PI - beta;
        var coord = std::safe_sqrt((view_zenith_cos_angle - zenith_horizon_angle) / beta);
        uv.y = coord;
    }

    uv.x = std::safe_sqrt(light_view_angle / PI);
    return from_unit_to_sub_uvs(uv, f32x2(sky_view_lut_size.xy));
}

public func uv_to_sky_view_lut_params(
    f32 atmos_radius,
    f32 planet_radius,
    i32x2 sky_view_lut_size,
    f32x2 uv,
    f32 altitude
) -> f32x2 {
    uv = from_sub_uvs_to_unit(uv, f32x2(sky_view_lut_size));
    var view_zenith_angle: f32;
    var light_view_angle: f32;
    if (altitude < atmos_radius) {
        let beta = asin(planet_radius / altitude);
        let zenith_horizon_angle = PI - beta;
        if (uv.y < 0.5) {
            let y = 1.0 - 2.0 * uv.y;
            let coord = 1.0 - y * y;
            view_zenith_angle = zenith_horizon_angle * coord;
        } else {
            let y = 2.0 * uv.y - 1.0;
            let coord = y * y;
            view_zenith_angle = zenith_horizon_angle + beta * coord;
        }
    } else {
        let beta = asin(atmos_radius / altitude);
        let zenith_horizon_angle = PI - beta;
        let coord = uv.y * uv.y;
        view_zenith_angle = zenith_horizon_angle + beta * coord;
    }

    light_view_angle = (uv.x * uv.x) * PI;
    return f32x2(view_zenith_angle, light_view_angle);
}

public func move_to_top_atmosphere(inout f32x3 pos, f32x3 dir, f32 atmos_radius) -> bool {
    f32 h = length(pos);
    if (h > atmos_radius) {
        const let top_intersection = std::ray_sphere_intersect_nearest(pos, dir, atmos_radius);
        if (top_intersection != -1.0) {
            f32x3 up_vec = pos / h;
            f32x3 up_offset = up_vec * -PLANET_RADIUS_OFFSET;
            pos = pos + dir * top_intersection + up_offset;
        } else {
            return false;
        }
    }
    return true;
}

public struct MediumScattering {
    public f32x3 mie_scattering;
    public f32x3 rayleigh_scattering;
    public f32x3 extinction_sum;

    [ForceInline]
    public __init(in Environment environment, f32 altitude) {
        let mie_density = exp(-altitude / environment.atmos_mie_density);
        let rayleigh_density = exp(-altitude / environment.atmos_rayleigh_density);
        let ozone_density = max(0.0, 1.0 - abs(altitude - environment.atmos_ozone_height) / environment.atmos_ozone_thickness);

        let mie_extinction = environment.atmos_mie_extinction * mie_density;
        let rayleigh_extinction = environment.atmos_rayleigh_scatter * rayleigh_density;
        let ozone_extinction = environment.atmos_ozone_absorption * ozone_density;

        this.mie_scattering = environment.atmos_mie_scatter * mie_density;
        this.rayleigh_scattering = environment.atmos_rayleigh_scatter * rayleigh_density;
        this.extinction_sum = mie_extinction + rayleigh_extinction + ozone_extinction;
    }
};

public struct AtmosphereLuminance {
    public f32x3 luminance = 0.0;
    public f32x3 multiscattering_as_1 = 0.0;
};

public struct AtmosphereIntegrateInfo {
    public f32x3 eye_pos = {};
    public f32x3 eye_dir = {};
    public f32x3 sun_dir = {};
    public f32 sun_intensity = 10.0;
    public f32 step_count = 32.0;
    public constexpr bool eval_planet_luminance = false;
    public constexpr bool eval_multiscattering = false;
};

public func integrate_single_scattered_luminance(
    in AtmosphereIntegrateInfo info,
    in Environment environment,
    in Sampler lut_sampler,
    in Image2D<f32x4> transmittance_image = {},
    in Image2D<f32x4> multiscattering_image = {}
) -> AtmosphereLuminance {
    AtmosphereLuminance result = {};

    if (dot(info.eye_pos, info.eye_pos) <= environment.atmos_planet_radius * environment.atmos_planet_radius) {
        return result;
    }

    let planet_intersection = std::ray_sphere_intersect_nearest(info.eye_pos, info.eye_dir, environment.atmos_planet_radius);
    let atmos_intersection = std::ray_sphere_intersect_nearest(info.eye_pos, info.eye_dir, environment.atmos_atmos_radius);
    var integration_length = 0.0;
    if (atmos_intersection == -1.0) {
        // No intersection
        return result;
    } else if (planet_intersection == -1.0) {
        // Atmosphere only intersection
        integration_length = atmos_intersection;
    } else {
        integration_length = max(0.0, planet_intersection);
    }

    let cos_theta = dot(info.sun_dir, info.eye_dir);
    let rayleigh_phase = std::rayleigh_phase(cos_theta);
    let mie_phase = std::henyey_greenstein_draine_phase(environment.atmos_mie_asymmetry, cos_theta);

    var transmittance_sum = f32x3(1.0);
    var step_length = integration_length / info.step_count;
    var old_ray_shift = 0.0;
    for (f32 step = 0.0; step < info.step_count; step += 1.0) {
        var new_ray_shift = integration_length * (step + 0.3) / info.step_count;
        step_length = new_ray_shift - old_ray_shift;
        old_ray_shift = new_ray_shift;

        let step_pos = info.eye_pos + new_ray_shift * info.eye_dir;

        let h = length(step_pos);
        let altitude = h - environment.atmos_planet_radius;
        let medium_info = MediumScattering(environment, altitude);
        let scattering_sum = medium_info.rayleigh_scattering + medium_info.mie_scattering;

        let up = normalize(step_pos);
        let sun_theta = dot(info.sun_dir, up);

        let transmittance_uv = transmittance_params_to_lut_uv(
            environment.atmos_atmos_radius, environment.atmos_planet_radius, f32x2(h, sun_theta));
        let sun_transmittance = transmittance_image.SampleLevel(lut_sampler, transmittance_uv, 0.0).rgb;

        var MS = f32x3(0.0);
        if (info.eval_multiscattering) {
            let multiscatter_uv = multiscattering_params_to_lut_uv(
                environment.atmos_atmos_radius, environment.atmos_planet_radius, environment.multiscattering_lut_size.xy, altitude, sun_theta);
            MS = multiscattering_image.SampleLevel(lut_sampler, multiscatter_uv, 0.0).rgb;
        }

        var scattering_phase = medium_info.mie_scattering * mie_phase + medium_info.rayleigh_scattering * rayleigh_phase;
        let earth_shadow = std::ray_sphere_intersect_nearest(step_pos, info.sun_dir, environment.atmos_planet_radius) == -1.0 ? 1.0 : 0.0;
        let sun_luminance = earth_shadow * sun_transmittance * scattering_phase + (MS * scattering_sum);
        let step_transmittance = exp(-step_length * medium_info.extinction_sum);

        var integral = (sun_luminance - sun_luminance * step_transmittance) / medium_info.extinction_sum;
        var ms_integral = (scattering_sum - scattering_sum * step_transmittance) / medium_info.extinction_sum;

        let extinction_zero = medium_info.extinction_sum == f32x3(0.0);
        integral = select(extinction_zero, f32x3(0.0), integral);
        ms_integral = select(extinction_zero, f32x3(0.0), ms_integral);

        result.luminance += integral * transmittance_sum;
        result.multiscattering_as_1 += ms_integral * transmittance_sum;
        transmittance_sum *= step_transmittance;
    }

    // Accumulate light bouncing off planet's ground
    if (info.eval_planet_luminance && integration_length != -1.0 && integration_length == planet_intersection) {
        let planet_pos = info.eye_pos + integration_length * info.eye_dir;
        let h = length(planet_pos);
        let up = planet_pos / h;
        let sun_theta = dot(info.sun_dir, up);
        let NoL = saturate(dot(normalize(info.sun_dir), normalize(up)));

        let transmittance_uv = transmittance_params_to_lut_uv(environment.atmos_atmos_radius, environment.atmos_planet_radius, f32x2(h, sun_theta));
        let sun_transmittance = transmittance_image.SampleLevel(lut_sampler, transmittance_uv, 0.0).rgb;

        result.luminance += sun_transmittance * transmittance_sum * NoL * environment.atmos_terrain_albedo / PI;
    }

    return result;
}

public func sample_aerial_perspective(
    in Image3D<f32x4> aerial_perspective_lut,
    in Sampler sampler,
    i32x3 aerial_perspective_lut_size,
    f32x2 uv,
    f32x3 camera_relative_pos,
    f32 aerial_perspective_start_km
) -> f32x4 {
    const let lut_size = f32x3(aerial_perspective_lut_size);
    const let per_slice_depth = f32(lut_size.x / lut_size.z);
    const let aerial_perspective_lut_depth = lut_size.z;

    let relative_depth = max(0.0, length(camera_relative_pos) - aerial_perspective_start_km);
    let linear_slice = relative_depth * rcp(per_slice_depth);
    let linear_w = linear_slice * rcp(aerial_perspective_lut_depth);
    let non_linear_w = sqrt(linear_w);
    let non_linear_slice = non_linear_w * aerial_perspective_lut_depth;

    f32 weight = 1.0;
    const let half_slice_depth = 0.70710678118654752440084436210485f; // sqrt(0.5f)
    if (non_linear_slice < half_slice_depth) {
        weight = saturate(non_linear_slice * non_linear_slice * 2.0);
    }

    const let near_depth_fade_out = 1.0 / 0.00001;
    weight *= saturate(relative_depth * near_depth_fade_out);

#if 0
    return f32x4(relative_depth, linear_slice, non_linear_w, linear_w);
#endif
    f32x4 aerial_perspective = aerial_perspective_lut.SampleLevel(sampler, f32x3(uv, non_linear_w), 0.0);
    aerial_perspective.xyz *= weight;
    aerial_perspective.w = 1.0 - (weight * (1.0 - aerial_perspective.w));

    return aerial_perspective;
}

public func get_atmosphere_illuminance_along_ray(
    f32x3 eye_pos,
    f32x3 eye_dir,
    f32x3 sun_dir,
    in Environment environment,
    in Sampler sampler,
    in Image2D<f32x4> sky_view_image
) -> f32x3 {
    let height = length(eye_pos);
    let view_zenith_cos_angle = acos(dot(eye_dir, f32x3(0.0, 1.0, 0.0)));
    let light_view_cos_angle = acos(clamp(dot(
        normalize(f32x3(sun_dir.x, 0.0, sun_dir.z)),
        normalize(f32x3(eye_dir.x, 0.0, eye_dir.z)),
    ), -1.0, 1.0));
    let planet_intersection = std::ray_sphere_intersect_nearest(eye_pos, eye_dir, environment.atmos_planet_radius);
    let uv = sky_view_params_to_lut_uv(
        environment.atmos_atmos_radius,
        environment.atmos_planet_radius,
        environment.sky_view_lut_size.xy,
        planet_intersection != -1.0,
        height,
        view_zenith_cos_angle,
        light_view_cos_angle);

    let result = sky_view_image.SampleLevel(sampler, uv, 0.0);
    let atmos_luminance = result.rgb * result.a;
    return atmos_luminance * environment.sun_intensity;
}