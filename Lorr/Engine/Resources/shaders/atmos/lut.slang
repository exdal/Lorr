import atmos;
import "../lorr";


struct VertexOutput {
    vec4 position     : SV_Position;
    vec2 tex_coord    : TEXCOORD;
};

[[vk::push_constant]] struct {
    World *world;
    ImageViewID transmittance_image_id;
    ImageViewID ms_image_id;
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
vec4 fs_main(VertexOutput input) : SV_TARGET
{
    Atmosphere *atmos = &c.world->atmosphere;
    vec3 eye_pos = vec3(0.0, atmos.planet_radius + c.world->cameras[0].position.y + 2.0, 0.0); // ALWAYS HAVE + 1 ON EYE POS!!!
    const vec3 sun_dir = c.world->sun.direction;
    const f32 sun_intensity = 10.0;
    const f32 step_count = 30.0;

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
    
    vec3 luminance = vec3(0.0, 0.0, 0.0);
    vec3 transmittance = vec3(1.0, 1.0, 1.0);
    
    f32 max_dist = 0.0;
    if (!ray_hit_sphere(eye_pos, rayDirection, atmos.planet_radius, max_dist)) {
        ray_hit_sphere(eye_pos, rayDirection, atmos.atmos_radius, max_dist);
    }

    f32 stepSize = max_dist / step_count;
    f32 t = 0.0;
    for (float i = 0.0; i < step_count; i += 1.0) {
        f32 nextT = stepSize * i;
        f32 deltaT = nextT - t;
        t = nextT;
        
        vec3 stepPosition = eye_pos + t * rayDirection;
        
        h = length(stepPosition);
        
        // Altitude from ground to top atmosphere
        f32 altitude = h - atmos.planet_radius;
        vec3 extinction = extinction_sum(atmos, altitude);
        vec3 altitudeTrans = exp(-deltaT * extinction);
        
        // Shadowing factor
        f32 sunTheta = dot(sun_dir, stepPosition / h);
        vec3 sunTrans = sample_lut(atmos, c.transmittance_image_id, c.world.linear_sampler, altitude, sunTheta);
        vec3 MS = sample_lut(atmos, c.ms_image_id, c.world.linear_sampler, altitude, sunTheta);
        //vec3 MS = vec3(0.0);
        // Get scattering coefficient
        vec3 rayleighScat;
        f32 mieScat;
        get_scattering(*atmos, altitude, rayleighScat, mieScat);
        
        // Molecules scattered on ray's position
        vec3 rayleighInScat = rayleighScat * (rayleighPhase + MS);
        vec3 mieInScat = mieScat * (miePhase + MS);
        vec3 scatteringPhase = (rayleighInScat + mieInScat) * sunTrans;

        // https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
        // slide 28
        vec3 integral = (scatteringPhase - scatteringPhase * altitudeTrans) / extinction;

        luminance += sun_intensity * (integral * transmittance);
        transmittance *= altitudeTrans;
    }

    return vec4(luminance, 1.0);
}

