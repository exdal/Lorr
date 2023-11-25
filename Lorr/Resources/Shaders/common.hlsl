#define u64 uint64_t
#define i64 int64_t
#define f32 float
#define f64 double
#define u32 uint
#define i32 int

#define LR_MAX_BINDLESS_LAYOUT_SIZE 8
#define PUSH_CONSTANT(name, body) \
    struct _##name body \
    uint __lr_descriptors[LR_MAX_BINDLESS_LAYOUT_SIZE]; \
    [[vk::push_constant]] ConstantBuffer<_##name> name