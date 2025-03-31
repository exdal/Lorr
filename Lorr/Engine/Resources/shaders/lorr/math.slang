implementing "../lorr";

/// CONSTANTS ///
// To sky unit, in our case meters to KM
public const static float CAMERA_SCALE_UNIT     = 0.01;
public const static float INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
public const static float PLANET_RADIUS_OFFSET  = 0.001;

///
/// PAHASE FUNCTIONS ///
///

public namespace lr {
public constexpr float uniform_phase() {
    return 1.0 / (4.0 * PI);
}

public float rayleigh_phase(float altitude) {
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

public float cornette_shanks_phase(float g, float altitude) {
    const float g2 = g * g;
    const float scale = 3.0 / (8.0 * PI);

    float num = (1.0 - g2) * (1.0 + altitude * altitude);
    float denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);

    return scale * num / denom;
}

public float henyey_greenstein_phase(float g, float cos_theta) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

///
/// GENERAL MATH ///
///

public float safe_sqrt(float x) {
    return sqrt(max(0, x));
}

public float2 ray_sphere_intersect(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;

    if (delta < 0.0) {
        return -1.0;
    }

    float sqrt_delta = sqrt(delta);
    return (-b + float2(-1.0, 1.0) * sqrt_delta) / (2.0 * a);
}

public Optional<float> ray_sphere_intersect_nearest(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;

    if (delta < 0.0) {
        return none;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);

    if (sol0 < 0.0 && sol1 < 0.0) {
        return none;
    }

    if (sol0 < 0.0) {
        return max(0.0, sol1);
    } else if (sol1 < 0.0) {
        return max(0.0, sol0);
    }

    return max(0.0, min(sol0, sol1));
}

public float ray_sphere_intersect_inside(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);
    return sol1;
}

public Optional<float2> ray_sphere_intersect_outside(float3 ray_origin, float3 ray_direction, float sphere_radius) {
    float a = dot(ray_direction, ray_direction);
    float b = 2.0 * dot(ray_direction, ray_origin);
    float c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    float delta = b * b - 4.0 * a * c;
    if (delta < 0.0 || a == 0.0) {
        return none;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);

    if (sol1 <= 0.0 || sol0 <= 0.0) {
        return none;
    }

    return float2(sol0, sol1);
}

public bool ray_rect_intersect(float2 bottom_left_0, float2 top_right_0, float2 bottom_left_1, float2 top_right_1) {
  return !(any(top_right_0 < bottom_left_1) || any(bottom_left_0 > top_right_1));
}

// HLSL fmod is not same as GLSL mod!!!!!!!
public float3 mod(float3 x, float3 y) {
  return x - y * floor(x / y);
}

// Unit vectors octahedral encoding
// "Survey of Efficient Representations for Independent Unit Vectors"
// http://jcgt.org/published/0003/02/01
public float3 oct_to_vec3(float2 e) {
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    const float2 sign_not_zero = float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero;
    }

    return normalize(v);
}

public float2 vec3_to_oct(float3 v) {
    float2 p = float2(v.x, v.y) * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    const float2 sign_not_zero = float2((p.x >= 0.0) ? 1.0 : -1.0, (p.y >= 0.0) ? 1.0 : -1.0);
    return (v.z <= 0.0) ? ((1.0 - abs(float2(p.y, p.x))) * sign_not_zero) : p;
}

// Color spaces

public float3 rec709_to_xyz(float3 color) {
	// clang-format off
	float3x3 mat = {
		0.4124564f, 0.3575761f, 0.1804375f,
		0.2126729f, 0.7151522f, 0.0721750f,
		0.0193339f, 0.1191920f, 0.9503041f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 rec2020_to_xyz(float3 color) {
	// clang-format off
	float3x3 mat = {
		0.636958f,  0.1446169f, 0.168881f,
		0.2627002f, 0.6779981f, 0.0593017f,
		0.f,        0.0280727f, 1.0609851f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 lms_to_xyz(float3 color) {
	// clang-format off
	float3x3 mat = {
		2.07018f, -1.326457f, 0.206616f,
		0.364988f, 0.680467f, -0.045422f,
		-0.0495955f, -0.0494212f, 1.187996f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 xyz_to_rec709(float3 color) {
	// clang-format off
	float3x3 mat = {
		3.2404542f, -1.5371385f, -0.4985314f,
		-0.9692660f, 1.8760108f, 0.0415560f,
		0.0556434f, -0.2040259f, 1.0572252f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 xyz_to_rec2020(float3 color) {
	// clang-format off
	float3x3 mat = {
		1.7166512f, -0.3556708f, -0.2533663f,
		-0.6666844f, 1.6164812f,  0.0157685f,
		0.0176399f, -0.0427706f,  0.9421031
	};
	// clang-format on
	return mul(mat, color);
}

public float3 xyz_to_lms(float3 color) {
	// clang-format off
	float3x3 mat = {
		0.3592f, 0.6976f, -0.0358f,
		-0.1922f, 1.1004f, 0.0755f,
		0.007f, 0.0749f, 0.8434f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 rec2020_to_ICtCp(float3 color) {
	color = xyz_to_lms(rec2020_to_xyz(color));
	color = pq_oetf(color * 200.f);
	// clang-format off
	float3x3 mat = {
		0.5f, 0.5f, 0.f,
		1.6137f, -3.3234f, 1.7097f,
		4.378f, -4.2455f, -0.1325f
	};
	// clang-format on
	return mul(mat, color);
}

public float3 ICtCp_to_rec2020(float3 color) {
	// clang-format off
	float3x3 mat = {
		1.f, 0.008605f, 0.1110356f,
		1.f, -0.008605f, -0.1110356f,
		1.f, 0.56004886f, -0.320637f
	};
	// clang-format on
	color = mul(mat, color);
	color = pq_eotf(color) / 200.f;
	return xyz_to_rec2020(lms_to_xyz(color));
}

public float3 xyz_to_Yxy(float3 color) {
	let inv = 1.f / dot(color, float3(1.f));
	return float3(color.y, color.xy * inv);
}

public float3 Yxy_to_xyz(float3 color) {
	return float3(color.x * color.y / color.z, color.x, color.x * (1.f - color.y - color.z) / color.z);
}

public float3 rec709_to_rec2020(float3 color) {
	return xyz_to_rec2020(rec709_to_xyz(color));
}

public float3 rec2020_to_rec709(float3 color) {
	return xyz_to_rec709(rec2020_to_xyz(color));
}

public float3 rec709_oetf(float3 color) {
	let mask = color < 0.018f;
	let a = color * 4.5f;
	let b = 1.099f * pow(color, 0.45f) - 0.099f;
	return select(mask, a, b);
}

public float3 rec709_eotf(float3 color) {
	let mask = color < 0.081f;
	let a = color / 4.5f;
	let b = pow((color + 0.099f) / 1.099f, 2.22f);
	return select(mask, a, b);
}

static const f32 M1 = 1305.f / 8192.f;
static const f32 M2 = 2523.f / 32.f;
static const f32 C1 = 107.f / 128.f;
static const f32 C2 = 2413.f / 128.f;
static const f32 C3 = 2392.f / 128.f;

public float3 pq_oetf(float3 color) {
	let y = color / 10000.f;
	let ym1 = pow(y, M1);
	return pow((C1 + C2 * ym1) / (1.f + C3 * ym1), M2);
}

public float3 pq_eotf(float3 color) {
	let ym2 = pow(color, 1.f / M2);
	return 10000.f * pow((max(ym2 - C1, 0.f)) / (C2 - C3 * ym2), 1.f / M1);
}

public f32 luminance_rec709(float3 color) {
	return rec709_to_xyz(color).y;
}

public f32 luminance_rec2020(float3 color) {
	return rec2020_to_xyz(color).y;
}

}
