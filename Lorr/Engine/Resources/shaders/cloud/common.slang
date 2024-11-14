implementing cloud;

public float remap(float orig, float old_min, float old_max, float new_min, float new_max) {
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

// WARN: These hashes are extremely fragile. Replacing one of them would collapse all noise functions.
public float hash13(float3 p) {
    const static uint3 VPRIME = uint3(3480082861U, 2420690917U, 2149110343U);
    const static uint FPRIME = 1317666547U;
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * UIF;
}

public float3 hash33(float3 p) {
    const static uint UI0 = 1597334673U;
    const static uint UI1 = 3812015801U;
    const static uint2 UI2 = uint2(UI0, UI1);
    const static uint3 UI3 = uint3(UI0, UI1, 2798796415U);
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z) * UI3;
    return float3(q) * UIF;
}

