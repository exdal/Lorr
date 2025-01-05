// DLS for ECS data reflection.
//
// Paste these macros into Component header:
/*
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
*/

// clang-format off
#ifdef ECS_EXPORT_TYPES
#define ECS_COMPONENT_BEGIN(name, ...) struct name {
#define ECS_COMPONENT_END(...) }
#define ECS_COMPONENT_MEMBER(name, type, ...) type name __VA_OPT__(=) __VA_ARGS__;

#ifndef ECS_COMPONENT_TAG
#define ECS_COMPONENT_TAG(name, ...) struct name {}
#endif
#endif
// clang-format on
