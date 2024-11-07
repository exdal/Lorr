import atmos;
import "../lorr";


struct VertexOutput {
    vec4 position     : SV_Position;
    vec2 tex_coord    : TEXCOORD;
};

[[vk::push_constant]] struct {
    World *world;
    ImageView transmittance_image;
    ImageView ms_image;
    u32 pad;
} c;

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = vec2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = vec4(output.tex_coord * vec2(2.0, 2.0) - vec2(1.0, 1.0), 0.5, 1);

    return output;
}

[shader("fragment")]
vec4 fs_main(VertexOutput input) : SV_TARGET {
    Camera *camera = &c.world->cameras[c.world->active_camera_index];
    Atmosphere *atmos = &c.world->atmosphere;
    Sun *sun = &c.world->sun;

    float eye_height = camera.position.y * CAMERA_SCALE_UNIT + PLANET_RADIUS_OFFSET;
    vec3 eye_pos = float3(0.0, eye_height + atmos.planet_radius, 0.0);
    const vec3 sun_dir = c.world->sun.direction;
    const i32 step_count = 30;

    f32 u = input.tex_coord.x;
    f32 v = input.tex_coord.y;

    // Non-linear parameterization
    if (v < 0.5) {
        f32 coord = 1.0 - 2.0 * v;
        v = coord * coord;
    } else {
        f32 coord = v * 2.0 - 1.0;
        v = -coord * coord;
    }

    f32 h = length(eye_pos);
    f32 azimuth_angle = 2.0 * PI * u;  // Consider 360 degrees.
    f32 horizon_angle = acos(sqrt(h * h - atmos.planet_radius * atmos.planet_radius) / h) - 0.5 * PI;
    f32 altitude_angle = v * 0.5 * PI - horizon_angle;

    f32 cosAltitude = cos(altitude_angle);
    vec3 rayDirection = vec3(
        cosAltitude * cos(azimuth_angle),
        sin(altitude_angle),
        cosAltitude * sin(azimuth_angle)
    );

    // Get Rayleigh + Mie phase
    f32 cosTheta = dot(rayDirection, sun_dir);
    f32 rayleighPhase = get_rayleigh_phase(-cosTheta);
    f32 miePhase = get_mie_phase(atmos, cosTheta);

    f32 max_dist = 0.0;
    if (!ray_hit_sphere(eye_pos, rayDirection, atmos.planet_radius, max_dist)) {
        ray_hit_sphere(eye_pos, rayDirection, atmos.atmos_radius, max_dist);
    }

    vec3 luminance = vec3(0.0, 0.0, 0.0);
    vec3 transmittance = vec3(1.0, 1.0, 1.0);

    for (int i = 0; i < step_count; i++) {
        float cur_step = float(i) / step_count;
        float next_step = float(i + 1) / step_count;
        cur_step *= cur_step;
        next_step *= next_step;
        cur_step = cur_step * max_dist;
        next_step = next_step > 1.0 ? max_dist : next_step * max_dist;
        float delta_step = next_step - cur_step;
        float step = cur_step + delta_step * 0.3;

        vec3 stepPosition = eye_pos + step * rayDirection;
        h = length(stepPosition);

        // Altitude from ground to top atmosphere
        f32 altitude = h - atmos.planet_radius;
        const let medium = MediumScattering(atmos, altitude);

        // Shadowing factor
        float3 up_vec = normalize(stepPosition);
        f32 sun_theta = dot(sun_dir, up_vec);

        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, sun_theta));
        float3 sun_transmittance = c.transmittance_image.sample(c.world->linear_sampler, transmittance_uv).rgb;

        float2 multiscatter_uv = get_multiscattering_lut_uv(atmos, altitude, sun_theta);
        float3 MS = c.ms_image.sample(c.world->linear_sampler, multiscatter_uv).rgb;

        // Molecules scattered on ray's position
        float3 scattering_phase = medium.mie_scattering * miePhase + medium.rayleigh_scattering * rayleighPhase;
        float _ = 0;
        float earth_shadow = !ray_hit_sphere(stepPosition, sun_dir, atmos->planet_radius, _) ? 1.0 : 0.0;

        float3 sun_luminance = earth_shadow * sun_transmittance * scattering_phase + MS * (medium.rayleigh_scattering + medium.mie_scattering);

        vec3 step_transmittance = exp(-delta_step * medium.extinction_sum);
        // https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
        // slide 28
        vec3 integral = (sun_luminance - sun_luminance * step_transmittance) / medium.extinction_sum;

        luminance += c.world->sun.intensity * (integral * transmittance);
        transmittance *= step_transmittance;
    }

    return vec4(luminance, 1.0);
}

