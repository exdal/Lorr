implementing cloud;

public float remap(float orig, float old_min, float old_max, float new_min, float new_max) {
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

#define FPRIME 1317666547U
#define VPRIME uint3(3480082861U, 2420690917U, 2149110343U)
#define SMALLESTFLOAT (1.0 / float(0xffffffffU))
public float hash13(float3 p) {
    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * SMALLESTFLOAT;
}

#undef FPRIME
#undef VPRIME
#undef SMALLESTFLOAT

// 3 in 3 out hash from 
#define UI0 1597334673U
#define UI1 3812015801U
#define UI3 uint3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))
public float3 hash33(float3 p) {
    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z)*UI3;
    return float3(q) * UIF;
}

#undef UI0
#undef UI1
#undef UI3
#undef UIF

public float smoothstep_unchecked(float x) {
    x = clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}
