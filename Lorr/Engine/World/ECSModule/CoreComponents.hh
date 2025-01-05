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
    ECS_COMPONENT_MEMBER(position, glm::vec3)
    ECS_COMPONENT_MEMBER(scale, glm::vec3)
    ECS_COMPONENT_MEMBER(rotation, glm::vec3)
    ECS_COMPONENT_MEMBER(matrix, glm::mat4)
ECS_COMPONENT_END();

ECS_COMPONENT_BEGIN(Camera)
    ECS_COMPONENT_MEMBER(orientation, glm::quat)
    ECS_COMPONENT_MEMBER(projection, glm::mat4)
    ECS_COMPONENT_MEMBER(axis_velocity, glm::vec3)
    ECS_COMPONENT_MEMBER(fov, f32)
    ECS_COMPONENT_MEMBER(aspect_ratio, f32)
    ECS_COMPONENT_MEMBER(near_clip, f32)
    ECS_COMPONENT_MEMBER(far_clip, f32)
    ECS_COMPONENT_MEMBER(index, u32)
ECS_COMPONENT_END();

ECS_COMPONENT_TAG(EditorSelected);
ECS_COMPONENT_TAG(PerspectiveCamera);
ECS_COMPONENT_TAG(OrthographicCamera);
ECS_COMPONENT_TAG(ActiveCamera);
ECS_COMPONENT_TAG(EditorCamera);

ECS_COMPONENT_BEGIN(RenderingModel)
    ECS_COMPONENT_MEMBER(uuid, UUID)
    ECS_COMPONENT_MEMBER(model_id, ModelID)
ECS_COMPONENT_END();

// Any entity with this tag won't be serialized
ECS_COMPONENT_TAG(Hidden);

// clang-format on
