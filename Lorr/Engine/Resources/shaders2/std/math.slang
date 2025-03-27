implementing std;

import glm;

public namespace std {
public static constexpr f32 PI = 3.1415926535897932384626433832795;
public static constexpr f32 TAU = 6.283185307179586476925286766559;

/// PHASE FUNCTIONS
public constexpr func uniform_phase() -> f32 {
    return 1.0 / (4.0 * PI);
}

public func rayleigh_phase(f32 altitude) -> f32 {
    const f32 k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

public func cornette_shanks_phase(f32 g, f32 altitude) -> f32 {
    const f32 g2 = g * g;
    const f32 scale = 3.0 / (8.0 * PI);
    const f32 num = (1.0 - g2) * (1.0 + altitude * altitude);
    const f32 denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);

    return scale * num / denom;
}

public func henyey_greenstein_phase(f32 g, f32 cos_theta) -> f32 {
    const f32 g2 = g * g;
    const f32 denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

/// GENERAL MATH
public func safe_sqrt(f32 x) -> f32 {
    return sqrt(max(0, x));
}

public func mod(glm::vec3 x, glm::vec3 y) -> glm::vec3 {
  return x - y * floor(x / y);
}

/// INTERSECTION
public func ray_sphere_intersect(glm::vec3 ray_origin, glm::vec3 ray_direction, f32 sphere_radius) -> Optional<glm::vec2> {
    const let a = dot(ray_direction, ray_direction);
    const let b = 2.0 * dot(ray_direction, ray_origin);
    const let c = dot(ray_origin, ray_origin) - (sphere_radius * sphere_radius);
    const let delta = b * b - 4.0 * a * c;

    if (delta < 0.0) {
        return none;
    }

    const let sqrt_delta = sqrt(delta);
    return (-b + glm::vec2(-1.0, 1.0) * sqrt_delta) / (2.0 * a);
}

public func ray_sphere_intersect_nearest(glm::vec3 ray_origin, glm::vec3 ray_direction, f32 sphere_radius) -> Optional<f32> {
    const let sol = ray_sphere_intersect(ray_origin, ray_direction, sphere_radius);
    if (!sol.hasValue) {
        return none;
    }

    const let sol0 = sol.value.x;
    const let sol1 = sol.value.y;

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

public func ray_sphere_intersect_inside(glm::vec3 ray_origin, glm::vec3 ray_direction, f32 sphere_radius) -> Optional<f32> {
    const let sol = ray_sphere_intersect(ray_origin, ray_direction, sphere_radius);
    if (!sol.hasValue) {
        return none;
    }

    return sol.value.y;
}

public func ray_sphere_intersect_outside(glm::vec3 ray_origin, glm::vec3 ray_direction, f32 sphere_radius) -> Optional<glm::vec2> {
    const let sol = ray_sphere_intersect(ray_origin, ray_direction, sphere_radius);
    if (!sol.hasValue) {
        return none;
    }

    const let sol0 = sol.value.x;
    const let sol1 = sol.value.y;
    if (sol1 <= 0.0 || sol0 <= 0.0) {
        return none;
    }

    return glm::vec2(sol0, sol1);
}

public func ray_rect_intersect(glm::vec2 bottom_left_0, glm::vec2 top_right_0, glm::vec2 bottom_left_1, glm::vec2 top_right_1) -> bool {
  return !(any(top_right_0 < bottom_left_1) || any(bottom_left_0 > top_right_1));
}

}
