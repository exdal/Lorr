import "../lorr";

bool ray_hit_sphere(float3 origin, float3 dir, float radius, out float t) {
    float a = dot(dir, dir);
    float b = 2.0 * dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant > 0.0) {
        t = (-b - sqrt(discriminant)) / (a * 2.0);
        if (t > 0.0) {
            return true;
        }
        t = (-b + sqrt(discriminant)) / (a * 2.0);
        return t > 0.0;
    }

    t = -1.0;
    return false;
}

float3 extinction_sum(lr::Atmosphere *atmos, float altitude) {
    float3 rayleigh = atmos->rayleigh_scatter * exp(-altitude / atmos->rayleigh_density);
    float mie = (atmos.mie_scatter + atmos.mie_absorption) * exp(-altitude / atmos->mie_density);
    float3 ozone = atmos->ozone_absorption * max(0.0, 1.0 - abs(altitude - atmos->ozone_height) / atmos->ozone_thickness);

    return rayleigh + mie + ozone;
}

float get_rayleigh_phase(float altitude)
{
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

float get_mie_phase(lr::Atmosphere *atmos, float altitude)
{
    const float g = atmos->mie_asymmetry;
    const float g2 = g * g;
    const float scale = 3.0 / (8.0 * PI);

    float num = (1.0 - g2) * (1.0 + altitude * altitude);
    float denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);
    
    return scale * num / denom;
}

void get_scattering(lr::Atmosphere *atmos, float altitude, out float3 rayleigh, out float mie)
{
    rayleigh = atmos->rayleigh_scatter * exp(-altitude / atmos->rayleigh_density);
    mie = (atmos->mie_scatter + atmos->mie_absorption) * exp(-altitude / atmos->mie_density);
}

float3 sample_lut(lr::Atmosphere *atmos, ImageViewID target_image_id, SamplerID sampler_id, float altitude, float theta)
{
    const Texture2D target_image = Texture2D<float4>::get(target_image_id);
    const SamplerState sampler = SamplerState::get(sampler_id);

    float2 uv = float2(0.5 + 0.5 * theta, max(0.0, min(altitude / (atmos->atmos_radius - atmos->planet_radius), 1.0)));
    return target_image.SampleLevel(sampler, uv, 0.0).xyz;
}
