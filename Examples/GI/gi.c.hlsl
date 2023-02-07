#define PHI 1.61803398874989484820459
#define PI 3.141592653589793238462643
#define MAX_BOUNCES 5
#define FAR_MAX 1e8

struct Triangle
{
    float4 Positions[3];
    float4 Normal;
    float4 Color;
};

struct _PushConstants
{
    float3 CameraPos;
    uint Width;
    uint Height;
    uint Time;
    uint TriangleCount;
};

[[vk::binding(0, 0)]] RWTexture2D<float4> u_TextureOut : register(u0, space0);
[[vk::binding(1, 0)]] StructuredBuffer<Triangle> u_SceneData : register(t1, space0);
[[vk::push_constant]] ConstantBuffer<_PushConstants> PushConstants : register(b0, space1);


float gold_noise(float2 xy, float seed)
{
    return frac(tan(distance(xy * PHI, xy) * seed) * xy.x);
}

uint base_hash(uint2 p) 
{
    p = 1103515245U * ((p >> 1U) ^ (p.yx));
    uint h32 = 1103515245U * ((p.x) ^ (p.y >> 3U));
    return h32 ^ (h32 >> 16);
}

float2 hash2(float seed) 
{
    uint n = base_hash(asuint(float2(seed, seed)));
    uint2 rz = uint2(n, n * 48271U);
    return float2(rz.xy & uint2(0x7fffffffU, 0x7fffffffU))/float(0x7fffffffU);
}

float4 greaterThanEqual(float4 a, float4 b)
{
	return float4(
		a.x >= b.x,
		a.y >= b.y,
		a.z >= b.z,
		a.w >= b.w
	);
}

float3 GetRayBounceDirection(float3 n, float seed)
{
	float2 u = hash2(seed);

    float r = sqrt(u.x);
    float theta = 2.0 * PI * u.y;
 
    float3 B = normalize(cross(n, float3(0.0, 1.0, 1.0)));
	float3 T = cross(B, n);
    
    return normalize(r * sin(theta) * B + sqrt(1.0 - u.x) * n + r * cos(theta) * T);
}

struct Camera
{
    float3 Origin;
    float3 U;
    float3 V;
    float3 W;
};

struct Intersection
{
    int ID;
    float T;
    float3 Pos;
};

Camera GetCamera(float3 origin, float3 target, float3 up) 
{
    Camera c;
    c.Origin = origin;
    c.W = normalize(target);
    c.U = normalize(cross(c.W, up));
    c.V = normalize(cross(c.U, c.W));
    
    return c;
}

float3 GetRayDirection(Camera c, float2 uv, float aspect, float fov)
{
    float halfAngle = radians(fov) / 2.0;
	float hSize = tan(halfAngle);
	float d = 1.0;
	float left = -hSize;
	float right = hSize;
	float top = hSize;
	float bottom = -hSize;

    float x = bottom + (top - bottom) * uv.y;
	float y = (left + (right - left) * uv.x) * aspect;
	float3 dir = (d * c.W) + x * c.V + y * c.U;

    return normalize(dir);
}

Intersection IntersectTriangle(float3 origin, float3 direction) 
{
    Intersection it;
    it.T = FAR_MAX;

    int idx = -1;
    float triT = FAR_MAX;
    float3 triCoords = 0.0;

    for(uint i = 0; i < PushConstants.TriangleCount; i++)
    {
        Triangle tri = u_SceneData[i];

        float3 A = tri.Positions[0].xyz;
        float3 E0 = tri.Positions[1].xyz - A; 
        float3 E1 = tri.Positions[2].xyz - A;
        float3 p = cross(direction, E1);

        float det = dot(p, E0);
        if (det < 1e-2)
        {
            continue;
        }

        float3 t = origin - A;
        float3 q = cross(t, E0);

        float4 uvt = float4(dot(t, p), dot(direction, q), dot(E1, q), 0.0) / det;
        uvt.w = 1.0 - uvt.x - uvt.y;

        if (all(greaterThanEqual(uvt, float4(0.0, 0.0, 0.0, 0.0))) && uvt.z < triT)
        {
            idx = i;
            triT = uvt.z;
            triCoords = uvt.ywx;

            Triangle tri = u_SceneData[idx];
            it.ID = idx;
            it.T = triT;
            it.Pos = tri.Positions[2].xyz * triCoords.x + 
                    tri.Positions[0].xyz * triCoords.y + 
                    tri.Positions[1].xyz * triCoords.z;
        }
    }

    return it;
}

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DISPATCHTHREADID)
{
    float2 res = float2(float(PushConstants.Width), float(PushConstants.Height));
    float2 uv = float2(threadID.xy) / res;
    float aspect = res.x / res.y;

    Camera c = GetCamera(PushConstants.CameraPos.xyz, float3(0.0, 0.0, 1.0), float3(0.0, 1.0, 0.0));
    float3 rayPos = c.Origin;
    float3 rayDir = GetRayDirection(c, uv, aspect, 37.0);
    
    float3 radiance = float3(0.0, 0.0, 0.0);
    float3 throughput = float3(1.0, 1.0, 1.0);

    for (uint i = 0; i < MAX_BOUNCES; i++)
    {
        float seed = i + PushConstants.Time + res.y * threadID.x / res.x + res.x * threadID.y / res.y;
        seed = gold_noise(uv, seed);

        Intersection t = IntersectTriangle(rayPos, rayDir);
        Triangle tri = u_SceneData[t.ID];

        if (t.T == FAR_MAX)
        {
            radiance += throughput * float3(0.52, 0.808, 0.922);
            break;
        }

        rayPos = t.Pos;
        rayDir = GetRayBounceDirection(tri.Normal.xyz, seed);
        
        if (tri.Color.w > 0.0)
            radiance += throughput * tri.Color.xyz * tri.Color.w;
        else
            throughput *= tri.Color.xyz;
    }

    if (PushConstants.Time != 0)
    {
        float weight = 1.0 / float(PushConstants.Time + 1);

        u_TextureOut[threadID.xy] = float4((1.0 - weight) * u_TextureOut[threadID.xy].xyz + weight * radiance, 1.0);
    }
    else 
    {
        u_TextureOut[threadID.xy] = float4(radiance, 1.0);
    }
}