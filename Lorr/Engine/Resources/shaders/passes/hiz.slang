module hiz;

import std;
import gpu;

// Do not remove this comment:
// Taken from: https://github.dev/SparkyPotato/radiance/blob/main/shaders/passes/mesh/hzb.slang,
// which is based on https://github.com/Themaister/Granite/blob/master/assets/shaders/post/hiz.comp,
// which is HiZ modification of AMD's Single Pass Downsampler.

[[vk::constant_id(0)]] const u32 MIP_COUNT = 0;
[[vk::constant_id(1)]] const u32 WORK_GROUP_COUNT = 0;
[[vk::constant_id(2)]] const u32 INPUT_WIDTH = 0;
[[vk::constant_id(3)]] const u32 INPUT_HEIGHT = 0;
static constexpr u32x2 INPUT_EXTENT = u32x2(INPUT_WIDTH, INPUT_HEIGHT);
static constexpr f32x2 INV_INPUT_EXTENT = 1.0 / f32x2(INPUT_EXTENT);

[[vk::binding(0)]] Sampler sampler;
[[vk::binding(1)]] Image2D<f32> src_image;

[[vk::binding(2)]]  StorageImage2D<f32> dst_mip_0;
[[vk::binding(3)]]  StorageImage2D<f32> dst_mip_1;
[[vk::binding(4)]]  StorageImage2D<f32> dst_mip_2;
[[vk::binding(5)]]  StorageImage2D<f32> dst_mip_3;
[[vk::binding(6)]]  StorageImage2D<f32> dst_mip_4;
[[vk::binding(7)]]  globallycoherent StorageImage2D<f32> dst_mip_5;
[[vk::binding(8)]]  StorageImage2D<f32> dst_mip_6;
[[vk::binding(9)]]  StorageImage2D<f32> dst_mip_7;
[[vk::binding(10)]] StorageImage2D<f32> dst_mip_8;
[[vk::binding(11)]] StorageImage2D<f32> dst_mip_9;
[[vk::binding(12)]] StorageImage2D<f32> dst_mip_10;
[[vk::binding(13)]] StorageImage2D<f32> dst_mip_11;
[[vk::binding(14)]] globallycoherent RWStructuredBuffer<u32> spd_global_atomic;

func get_mip_image(u32 mip) -> StorageImage2D<f32> {
    switch(mip) {
        case 0: return dst_mip_0;
        case 1: return dst_mip_1;
        case 2: return dst_mip_2;
        case 3: return dst_mip_3;
        case 4: return dst_mip_4;
        case 5: return dst_mip_5;
        case 6: return dst_mip_6;
        case 7: return dst_mip_7;
        case 8: return dst_mip_8;
        case 9: return dst_mip_9;
        case 10: return dst_mip_10;
        default: return dst_mip_11;
    }
}

func unswizzle_16x16(u32 i) -> u32x2 {
    let x0 = bitfieldExtract(i, 0, 1);
    let y01 = bitfieldExtract(i, 1, 2);
    let x12 = bitfieldExtract(i, 3, 2);
    let y23 = bitfieldExtract(i, 5, 2);
    let x3 = bitfieldExtract(i, 7, 1);
    return u32x2(bitfieldInsert(bitfieldInsert(x0, x12, 1, 2), x3, 3, 1), bitfieldInsert(y01, y23, 2, 2));
}

func reduce(f32x2 p) -> f32 {
    return min(p.x, p.y);
}

func reduce_2x2(f32x4 q) -> f32 {
    return reduce(f32x2(reduce(q.xy), reduce(q.zw)));
}

func store(u32x2 texel, u32 mip, f32 v) -> void {
    get_mip_image(mip).store(texel, v);
}

func store_2x2(u32x2 p, u32 mip, f32x4 v) -> void {
    store(p + u32x2(0, 0), mip, v.x);
    store(p + u32x2(1, 0), mip, v.y);
    store(p + u32x2(0, 1), mip, v.z);
    store(p + u32x2(1, 1), mip, v.w);
}

func load(u32x2 texel) -> f32 {
    f32x2 uv = texel * INV_INPUT_EXTENT + INV_INPUT_EXTENT;
    return src_image.sample_mip(sampler, uv, 0);
}

func load_2x2(u32x2 p) -> f32x4 {
    let x = load(p + u32x2(0, 0));
    let y = load(p + u32x2(1, 0));
    let z = load(p + u32x2(0, 1));
    let w = load(p + u32x2(1, 1));
    return f32x4(x, y, z, w);
}

func load_4x4(u32x2 p) -> f32x4x4 {
    let x = load_2x2(p + u32x2(0, 0));
    let y = load_2x2(p + u32x2(2, 0));
    let z = load_2x2(p + u32x2(0, 2));
    let w = load_2x2(p + u32x2(2, 2));
    return f32x4x4(x, y, z, w);
}

func load_mid_2x2(u32x2 p) -> f32x4 {
    let x = dst_mip_5.load(p + u32x2(0, 0));
    let y = dst_mip_5.load(p + u32x2(1, 0));
    let z = dst_mip_5.load(p + u32x2(0, 1));
    let w = dst_mip_5.load(p + u32x2(1, 1));
    return f32x4(x, y, z, w);
}

func load_mid_4x4(u32x2 p) -> f32x4x4 {
    let x = load_mid_2x2(p + u32x2(0, 0));
    let y = load_mid_2x2(p + u32x2(2, 0));
    let z = load_mid_2x2(p + u32x2(0, 2));
    let w = load_mid_2x2(p + u32x2(2, 2));
    return f32x4x4(x, y, z, w);
}

func reduce_mip(f32x4x4 v, u32x2 p, u32 mip) -> f32 {
    let d0 = reduce_2x2(v[0]);
    let d1 = reduce_2x2(v[1]);
    let d2 = reduce_2x2(v[2]);
    let d3 = reduce_2x2(v[3]);
    let ret = f32x4(d0, d1, d2, d3);
    store_2x2(p, mip, ret);
    return reduce_2x2(ret);
}

func reduce_mip_simd(u32x2 p, u32 lid, u32 mip, f32 d) -> f32 {
    var horiz = QuadReadAcrossX(d);
    var vert = QuadReadAcrossY(d);
    var diag = QuadReadAcrossDiagonal(d);
    store(p, mip, d);

    if (MIP_COUNT > mip + 1) {
        p >>= 1;
        d = reduce_2x2(f32x4(d, horiz, vert, diag));
        horiz = std::wave_shuffle_xor(d, 0b1000);
        vert = std::wave_shuffle_xor(d, 0b0100);
        diag = std::wave_shuffle_xor(d, 0b1100);
        if ((lid & 3) == 0)
            store(p, mip + 1, d);
    }

    return reduce_2x2(f32x4(d, horiz, vert, diag));
}

groupshared f32 inter[16];
groupshared bool is_last;

[[shader("compute")]]
[[numthreads(256, 1, 1)]]
func cs_main(u32x2 group_id : SV_GroupID, u32 group_index : SV_GroupIndex) -> void {
    let local_id = std::subgroup_id() * WaveGetLaneCount() + WaveGetLaneIndex();
    let p = unswizzle_16x16(local_id);

    var base_texel = group_id * 64 + p * 4;
    var v = load_4x4(base_texel);

    var d = reduce_mip(v, base_texel >> 1, 0);
    if (MIP_COUNT <= 1) {
        return;
    }

    d = reduce_mip_simd(base_texel >> 2, local_id, 1, d);
    if (MIP_COUNT <= 3) {
        return;
    }

    if ((local_id & 15) == 0) {
        inter[local_id >> 4] = d;
    }

    std::control_barrier(std::memory_order_acq_rel);

    if (local_id < 16) {
        d = reduce_mip_simd(group_id * 4 + p, local_id, 3, inter[local_id]);
    }

    if (MIP_COUNT <= 5) {
        return;
    }

    if (local_id == 0) {
        store(group_id, 5, d);
    }

    if (MIP_COUNT <= 6) {
        return;
    }

    AllMemoryBarrierWithGroupSync();
    // std::memory_barrier(std::memory_order_acq_rel, std::MemoryScope::CrossDevice, std::MemoryLocation::Image | std::MemoryLocation::Workgroup);
    if (local_id == 0) {
        is_last = std::atomic_add(spd_global_atomic[0], 1, std::memory_order_relaxed) == WORK_GROUP_COUNT;
    }

    std::control_barrier(std::memory_order_acq_rel);
    if (!is_last) {
        return;
    }

    base_texel = p * 4;
    d = reduce_mip(load_mid_4x4(base_texel), base_texel >> 1, 6);
    if (MIP_COUNT <= 7) {
        return;
    }

    d = reduce_mip_simd(p, local_id, 7, d);
    if (MIP_COUNT <= 9) {
        return;
    }

    if ((local_id & 15) == 0) {
        inter[local_id >> 4] = d;
    }

    std::control_barrier(std::memory_order_acq_rel);

    if (local_id < 16) {
        d = reduce_mip_simd(p, local_id, 9, inter[local_id]);
    }

    if (MIP_COUNT <= 11) {
        return;
    }

    if (local_id == 0) {
        store(u32x2(0, 0), 11, d);
    }
}
