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
    ECS_COMPONENT_MEMBER(scale, glm::vec3, {1.0f, 1.0f, 1.0f})
    ECS_COMPONENT_MEMBER(rotation, glm::vec3, {0.0f, 0.0f, 0.0f})
    ECS_COMPONENT_MEMBER(matrix, glm::mat4, {})
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(Camera)
    ECS_COMPONENT_MEMBER(orientation, glm::quat, {})
    ECS_COMPONENT_MEMBER(projection, glm::mat4, {})
    ECS_COMPONENT_MEMBER(axis_velocity, glm::vec3, {})
    ECS_COMPONENT_MEMBER(velocity_mul, f32, 1.0)
    ECS_COMPONENT_MEMBER(fov, f32, 65.0f)
    ECS_COMPONENT_MEMBER(aspect_ratio, f32, 1.777f)
    ECS_COMPONENT_MEMBER(near_clip, f32, 0.1f)
    ECS_COMPONENT_MEMBER(far_clip, f32, 1000.0f)
    ECS_COMPONENT_MEMBER(index, u32, 0)
ECS_COMPONENT_END();

ECS_COMPONENT_TAG(EditorSelected);
ECS_COMPONENT_TAG(PerspectiveCamera);
ECS_COMPONENT_TAG(OrthographicCamera);
ECS_COMPONENT_TAG(ActiveCamera);
ECS_COMPONENT_TAG(EditorCamera);

ECS_COMPONENT_BEGIN(RenderingModel)
    ECS_COMPONENT_MEMBER(uuid, UUID, {})
    ECS_COMPONENT_MEMBER(model_id, ModelID, ModelID::Invalid)
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(DirectionalLight)
    ECS_COMPONENT_MEMBER(direction, glm::vec2, { 90.0, 45.0 })
    ECS_COMPONENT_MEMBER(intensity, f32, 10.0)
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(Atmosphere)
    ECS_COMPONENT_MEMBER(rayleigh_scattering, glm::vec3, { 5.802f, 13.558f, 33.100f })
    ECS_COMPONENT_MEMBER(rayleigh_density, f32, 8.0)
    ECS_COMPONENT_MEMBER(mie_scattering, glm::vec3, { 3.996f, 3.996f, 3.996f })
    ECS_COMPONENT_MEMBER(mie_density, f32, 1.2f)
    ECS_COMPONENT_MEMBER(mie_extinction, f32, 4.44f)
    ECS_COMPONENT_MEMBER(ozone_absorption, glm::vec3, { 0.650f, 1.881f, 0.085f })
    ECS_COMPONENT_MEMBER(ozone_height, f32, 25.0f)
    ECS_COMPONENT_MEMBER(ozone_thickness, f32, 15.0f)
ECS_COMPONENT_END();

// Any entity with this tag won't be serialized
ECS_COMPONENT_TAG(Hidden);

// clang-format on
