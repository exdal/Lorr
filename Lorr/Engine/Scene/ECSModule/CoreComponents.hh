#ifndef ECS_COMPONENT_BEGIN
    #define ECS_COMPONENT_BEGIN(...)
#endif

#ifndef ECS_COMPONENT_END
    #define ECS_COMPONENT_END(...)
#endif

#ifndef ECS_COMPONENT_MEMBER
    #define ECS_COMPONENT_MEMBER(...)
#endif

#ifndef ECS_COMPONENT_TAG
    #define ECS_COMPONENT_TAG(...)
#endif

// clang-format off
ECS_COMPONENT_BEGIN(Transform)
    ECS_COMPONENT_MEMBER(position, glm::vec3, {0.0f, 0.0f, 0.0f})
    ECS_COMPONENT_MEMBER(rotation, glm::vec3, {0.0f, 0.0f, 0.0f})
    ECS_COMPONENT_MEMBER(scale, glm::vec3, {1.0f, 1.0f, 1.0f})
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(Camera)
    ECS_COMPONENT_MEMBER(fov, f32, 90.0f)
    ECS_COMPONENT_MEMBER(resolution, glm::vec2, {})
    ECS_COMPONENT_MEMBER(near_clip, f32, 0.1f)
    ECS_COMPONENT_MEMBER(far_clip, f32, 1000.0f)
    ECS_COMPONENT_MEMBER(velocity, glm::vec3, { 0.0, 0.0, 0.0 })
    ECS_COMPONENT_MEMBER(max_velocity, f32, 1.0f)
    ECS_COMPONENT_MEMBER(accel_speed, f32, 1.0f)
    ECS_COMPONENT_MEMBER(decel_speed, f32, 1.0f)
    ECS_COMPONENT_MEMBER(acceptable_lod_error, f32, 2.0f)
ECS_COMPONENT_END();

ECS_COMPONENT_TAG(PerspectiveCamera);
ECS_COMPONENT_TAG(OrthographicCamera);
ECS_COMPONENT_TAG(ActiveCamera);

ECS_COMPONENT_BEGIN(RenderingMesh)
    ECS_COMPONENT_MEMBER(model_uuid, UUID, {})
    ECS_COMPONENT_MEMBER(mesh_index, u32, {})
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(Environment)
    ECS_COMPONENT_MEMBER(sun, bool, true)
    ECS_COMPONENT_MEMBER(sun_direction, glm::vec2, { 90.0, 45.0 })
    ECS_COMPONENT_MEMBER(sun_intensity, f32, 10.0)
    ECS_COMPONENT_MEMBER(atmos, bool, true)
    ECS_COMPONENT_MEMBER(atmos_rayleigh_scattering, glm::vec3, { 5.802f, 13.558f, 33.100f })
    ECS_COMPONENT_MEMBER(atmos_rayleigh_density, f32, 8.0)
    ECS_COMPONENT_MEMBER(atmos_mie_scattering, glm::vec3, { 3.996f, 3.996f, 3.996f })
    ECS_COMPONENT_MEMBER(atmos_mie_density, f32, 1.2f)
    ECS_COMPONENT_MEMBER(atmos_mie_extinction, f32, 4.44f)
    ECS_COMPONENT_MEMBER(atmos_mie_asymmetry, f32, 3.6f)
    ECS_COMPONENT_MEMBER(atmos_ozone_absorption, glm::vec3, { 0.650f, 1.881f, 0.085f })
    ECS_COMPONENT_MEMBER(atmos_ozone_height, f32, 25.0f)
    ECS_COMPONENT_MEMBER(atmos_ozone_thickness, f32, 15.0f)
    ECS_COMPONENT_MEMBER(atmos_aerial_perspective_start_km, f32, 8.0f)
    ECS_COMPONENT_MEMBER(eye_adaptation, bool, true)
    ECS_COMPONENT_MEMBER(eye_min_exposure, f32, -6.0f)
    ECS_COMPONENT_MEMBER(eye_max_exposure, f32, 18.0f)
    ECS_COMPONENT_MEMBER(eye_adaptation_speed, f32, 1.1f)
    ECS_COMPONENT_MEMBER(eye_iso, f32, 100.0f)
    ECS_COMPONENT_MEMBER(eye_k, f32, 12.5f)
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(VBGTAO)
    ECS_COMPONENT_MEMBER(thickness, f32, 0.25f)
    ECS_COMPONENT_MEMBER(depth_range_scale_factor, f32, 0.75f)
    ECS_COMPONENT_MEMBER(radius, f32, 0.5f)
    ECS_COMPONENT_MEMBER(radius_multiplier, f32, 1.457f)
    ECS_COMPONENT_MEMBER(slice_count, f32, 3.0f)
    ECS_COMPONENT_MEMBER(sample_count_per_slice, f32, 3.0f)
    ECS_COMPONENT_MEMBER(denoise_power, f32, 1.1f)
    ECS_COMPONENT_MEMBER(linear_thickness_multiplier, f32, 300.0f)
ECS_COMPONENT_END();

// clang-format on
