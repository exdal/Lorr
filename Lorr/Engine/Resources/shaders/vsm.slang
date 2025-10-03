import std;

/// VSM PAGE METADATA BITS (0-15)
[Flags]
public enum VSMPageState : u32 {
    // Is page visible?
    Visible,
    // Is page dirty (happens during "page faults", or cache hits)?
    Dirty,
    // Is page backed by physical image?
    Backed
};

// How many bits we reserve for metadata part?
public constexpr static u32 VSM_METADATA_BITS = 16u;
public constexpr static u32 VSM_METADATA_MASK = (1u << VSM_METADATA_BITS) - 1u;

/// VSM PAGE ADDRESS BITS (15-31)
public constexpr static u32 VSM_PAGE_PHYSICAL_ADDRESS_BITS = 16u;
public constexpr static u32 VSM_PAGE_PHYSICAL_ADDRESS_MASK = (1u << VSM_PAGE_PHYSICAL_ADDRESS_BITS) - 1u;

public struct VSMPageMetadata {
    public u32 data;

    [mutating]
    public __init(u32 data) {
        this.data = data;
    }

    public func is_visible() -> bool {
        return (this.data & VSMPageState.Visible) != 0;
    }

    [mutating]
    public func set_visible(bool v) -> void {
        // first we need to clear already set bit, and set the actual value we want
        this.data = (this.data & ~VSMPageState.Visible) | (VSMPageState.Visible * u32(v));
    }

    public func is_dirty() -> bool {
        return (this.data & VSMPageState.Dirty) != 0;
    }

    [mutating]
    public func set_dirty(bool v) -> void {
        this.data = (this.data & ~VSMPageState.Dirty) | (VSMPageState.Dirty * u32(v));
    }

    public func is_backed() -> bool {
        return (this.data & VSMPageState.Backed) != 0;
    }

    [mutating]
    public func set_backed(bool v) -> void {
        this.data = (this.data & ~VSMPageState.Backed) | (VSMPageState.Backed * u32(v));
    }

    public func get_physical_address() -> u32 {
        return this.data >> VSM_PAGE_PHYSICAL_ADDRESS_BITS;
    }

    public func get_physical_coords(u32 physical_page_image_size, u32 page_size) -> u32x2 {
        let physical_address = this.get_physical_address();
        let physical_page_count = physical_page_image_size / page_size;
        return u32x2(
            physical_address % physical_page_count,
            physical_address / physical_page_count
        );
    }

    [mutating]
    public func set_physical_address(u32 new_physical_address) -> void {
        this.data = (this.data & VSM_PAGE_PHYSICAL_ADDRESS_MASK) | ((new_physical_address & VSM_PAGE_PHYSICAL_ADDRESS_MASK) << 16u);
    }
};

public struct VSMAllocRequest {
    public i32x3 page_table_address;
};

public struct VSMPageAllocator {
    public u32 active_request_count;
    public u32 dirty_physical_page_count;
    public VSMAllocRequest *requests;
    public u32x2 *dirty_physical_page_coords;
};

public func push_page_alloc_request(__ref VSMPageAllocator allocator, in VSMAllocRequest request) -> void {
    let index = __atomic_add(allocator.active_request_count, 1, MemoryOrder::AcquireRelease);
    allocator.requests[index] = request;
}

public func push_dirty_physical_page(__ref VSMPageAllocator allocator, u32x2 physical_page_address) -> void {
    let index = __atomic_add(allocator.dirty_physical_page_count, 1, MemoryOrder::AcquireRelease);
    allocator.dirty_physical_page_coords[index] = physical_page_address;
}

public func vsm_page_coords_to_wrapped_coords(i32x2 page_coords, i32x2 page_offset, i32 page_table_size) -> i32x2 {
    let vsm_toroidal_pix_coords = page_coords.xy + page_offset;
    let vsm_wrapped_pix_coords = i32x2(std::mod(f32x2(vsm_toroidal_pix_coords), f32x2(page_table_size)));
    return vsm_wrapped_pix_coords;
}

public func vsm_clip_info_to_wrapped_coords(f32x2 uv, i32x2 page_offset, i32 page_table_size) -> i32x2 {
    if (any(uv <= 0.0) || any(uv >= 1.0)) {
        return i32x2(-1, -1);
    }

    let vsm_page_pix_coords = i32x2(floor(uv * f32(page_table_size)));
    return vsm_page_coords_to_wrapped_coords(vsm_page_pix_coords, page_offset, page_table_size);
}

public func virtual_uv_to_physical_texel(f32x2 virtual_uv, i32x2 physical_page_coords, i32 page_size, i32 physical_image_size) -> i32x2 {
    let virtual_texel_coord = i32x2(virtual_uv * f32(physical_image_size));
    let in_page_texel_coord = i32x2(std::mod(f32x2(virtual_texel_coord), f32x2(page_size)));
    let in_memory_offset = physical_page_coords * page_size;
    let memory_texel_coord = in_memory_offset + in_page_texel_coord;
    return memory_texel_coord;
}

public func calc_shadow_bias(u32 clipmap_index, f32 first_clipmap_texel_length, f32x3 N, f32x3 L) -> f32 {
    let clipmap_texel_size = exp2(f32(clipmap_index)) * first_clipmap_texel_length;
    let sqrt2 = 1.41421356;
    let q = 2.0 / 1048576.0;
    let b = sqrt2 * clipmap_texel_size * 0.5;
    return q + b * length(cross(N, L));
}
