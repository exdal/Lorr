import lorr;

struct Atmosphere {
    float3 rayleigh_scatter;
    float rayleigh_density;
    float planet_radius;
    float atmos_radius;
    float mie_scatter;
    float mie_absorption;
    float mie_density;
    float mie_asymmetry;
    float ozone_height;
    float ozone_thickness;
    float3 ozone_absorption;
};

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

float3 extinction_sum(Atmosphere atmos, float altitude) {
    float3 rayleigh = atmos.rayleigh_scatter * exp(-altitude / atmos.rayleigh_density);
    float mie = (atmos.mie_scatter + atmos.mie_absorption) * exp(-altitude / atmos.mie_density);
    float3 ozone = atmos.ozone_absorption * max(0.0, 1.0 - abs(altitude - atmos.ozone_height) / atmos.ozone_thickness);

    return rayleigh + mie + ozone;
}
