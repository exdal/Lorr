import "../lorr";
import "common";

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[[vk::push_constant]] struct {
    lr::World *world;
    ImageViewID transmittance_image_id;
    ImageViewID ms_image_id;
    SamplerID transmittance_sampler_id;
} c;

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    lr::Atmosphere *atmos = &c.world->atmosphere;
    float3 eye_pos = float3(0.0, atmos.planet_radius + c.world->cameras[0].position.y + 2.0, 0.0); // ALWAYS HAVE + 1 ON EYE POS!!!
    const float3 sun_dir = c.world->sun.direction;
    const float sun_intensity = 10.0;
    const float step_count = 48.0;

    float u = input.tex_coord.x;
    float v = input.tex_coord.y;

    // Non-linear parameterization
    if (v < 0.5) {
        float coord = 1.0 - 2.0 * v;
        v = coord * coord;
    } else {
        float coord = v * 2.0 - 1.0;
        v = -coord * coord;
    }

    float h = length(eye_pos);
    float azimuth_angle = 2.0 * PI * u;  // Consider 360 degrees.
    float horizon_angle = acos(sqrt(h * h - atmos.planet_radius * atmos.planet_radius) / h) - 0.5 * PI;
    float altitude_angle = v * 0.5 * PI - horizon_angle;

    float cosAltitude = cos(altitude_angle);
    float3 rayDirection = float3(
        cosAltitude * cos(azimuth_angle),
        sin(altitude_angle),
        cosAltitude * sin(azimuth_angle)
    );
    
    // Get Rayleigh + Mie phase
    float cosTheta = dot(rayDirection, sun_dir);
    float rayleighPhase = get_rayleigh_phase(-cosTheta);
    float miePhase = get_mie_phase(atmos, cosTheta);
    
    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);
    
    float max_dist = 0.0;
    if (!ray_hit_sphere(eye_pos, rayDirection, atmos.planet_radius, max_dist)) {
        ray_hit_sphere(eye_pos, rayDirection, atmos.atmos_radius, max_dist);
    }

    float stepSize = max_dist / step_count;
    float t = 0.0;
    for (float i = 0.0; i < step_count; i += 1.0)
    {
        float nextT = stepSize * i;
        float deltaT = nextT - t;
        t = nextT;
        
        float3 stepPosition = eye_pos + t * rayDirection;
        
        h = length(stepPosition);
        
        // Altitude from ground to top atmosphere
        float altitude = h - atmos.planet_radius;
        float3 extinction = extinction_sum(atmos, altitude);
        float3 altitudeTrans = exp(-deltaT * extinction);
        
        // Shadowing factor
        float sunTheta = dot(sun_dir, stepPosition / h);
        float3 sunTrans = sample_lut(atmos, c.transmittance_image_id, c.transmittance_sampler_id, altitude, sunTheta);
        float3 MS = sample_lut(atmos, c.ms_image_id, c.transmittance_sampler_id, altitude, sunTheta);
        //float3 MS = float3(0.0);
        // Get scattering coefficient
        float3 rayleighScat;
        float mieScat;
        get_scattering(*atmos, altitude, rayleighScat, mieScat);
        
        // Molecules scattered on ray's position
        float3 rayleighInScat = rayleighScat * (rayleighPhase + MS);
        float3 mieInScat = mieScat * (miePhase + MS);
        float3 scatteringPhase = (rayleighInScat + mieInScat) * sunTrans;

        // https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
        // slide 28
        float3 integral = (scatteringPhase - scatteringPhase * altitudeTrans) / extinction;

        luminance += sun_intensity * (integral * transmittance);
        transmittance *= altitudeTrans;
    }

    return float4(luminance, 1.0);
}

