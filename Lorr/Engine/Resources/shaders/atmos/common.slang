implementing atmos;

import "../lorr";

public bool ray_hit_sphere(float3 origin, float3 direction, float radius, out float t) {
    float a = dot(direction, direction);
    float b = 2.0 * dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    t = 0.0;
    if (discriminant < 0.0)
        return false;

    if (c <= 0.0)
        t = (-b + sqrt(discriminant)) / (a * 2.0);
    else 
        t = (-b + -sqrt(discriminant)) / (a * 2.0);
    
    return (b <= 0.0);
}

public float3 extinction_sum(Atmosphere *atmos, float altitude) {
    float3 rayleigh = atmos->rayleigh_scatter * exp(-altitude / atmos->rayleigh_density);
    float3 mie = (atmos->mie_scatter + atmos->mie_absorption) * exp(-altitude / atmos->mie_density);
    float3 ozone = atmos->ozone_absorption * max(0.0, 1.0 - abs(altitude - atmos->ozone_height) / atmos->ozone_thickness);

    return rayleigh + mie + ozone;
}

public float get_rayleigh_phase(float altitude)
{
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

public float get_mie_phase(Atmosphere *atmos, float altitude)
{
    const float g = atmos->mie_asymmetry;
    const float g2 = g * g;
    const float scale = 3.0 / (8.0 * PI);

    float num = (1.0 - g2) * (1.0 + altitude * altitude);
    float denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);
    
    return scale * num / denom;
}

public void get_scattering(Atmosphere atmos, float altitude, out float3 rayleigh, out float mie)
{
    rayleigh = atmos.rayleigh_scatter * exp(-altitude / atmos.rayleigh_density);
    mie = (atmos.mie_scatter + atmos.mie_absorption) * exp(-altitude / atmos.mie_density);
}

public float3 sample_lut(Atmosphere *atmos, ImageViewID target_image_id, SamplerID sampler_id, float altitude, float theta)
{
    const let lut_image = ImageView(target_image_id);

    float2 uv = float2(0.5 + 0.5 * theta, max(0.0, min(altitude / (atmos->atmos_radius - atmos->planet_radius), 1.0)));
    return lut_image.sample(sampler_id, uv).xyz;
}
