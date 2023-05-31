#define u64 uint64_t
#define i64 int64_t
#define f32 float
#define f64 double
#define u32 uint
#define i32 int

#define LR_MAX_BINDLESS_LAYOUT_SIZE 8 // count of 'int's
struct _lr_bindless_layout
{
    u32 Data[LR_MAX_BINDLESS_LAYOUT_SIZE];
};

#define LR_INIT_SHADER_WITH_CONSTANTS(target_struct, name) \
    layout(push_constant, scalar) uniform LR_PUSH_CONSTANT \
    {                                                      \
        _lr_bindless_layout lr_bindless_layout;            \
        target_struct name;                                \
    }

#define LR_INIT_SHADER()                                   \
    layout(push_constant, scalar) uniform LR_PUSH_CONSTANT \
    {                                                      \
        _lr_bindless_layout lr_bindless_layout;            \
    }
    
layout(scalar, set = 0, binding = 0) restrict readonly buffer _lr_buffer_ref
{
    uint64_t Address[];
} lr_buffer_ref;

layout(set = 1, binding = 0) uniform texture2D u_Images[];
// TODO: storage images
layout(set = 3, binding = 0) uniform sampler u_Samplers[];

u32 lr_get_descriptor_id(in _lr_bindless_layout bindlessLayout, u32 layoutID)
{
    return bindlessLayout.Data[layoutID];
}

#define LR_GET_BUFFER(struct_name, id) struct_name(lr_buffer_ref.Address[lr_get_descriptor_id(lr_bindless_layout, id)])
#define LR_GET_IMAGE(id) u_Images[lr_get_descriptor_id(lr_bindless_layout, id)]
#define LR_GET_SAMPLER(id) u_Samplers[lr_get_descriptor_id(lr_bindless_layout, id)]