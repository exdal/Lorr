import std;

/// VSM PAGE METADATA BITS (0-15)
// Is page visible?
constexpr static u32 VSM_PAGE_VISIBLE_BIT = 1u;
// Is page dirty (happens during "page faults", or cache hits)?
constexpr static u32 VSM_PAGE_DIRTY_BIT = 2u;
// Is page backed by physical image?
constexpr static u32 VSM_PAGE_BACKED_BIT = 4u;
// How many bits we reserve for metadata part?
constexpr static u32 VSM_METADATA_BITS = 16u;
constexpr static u32 VSM_METADATA_MASK = (1u << VSM_METADATA_BITS) - 1u;

/// VSM PAGE ADDRESS BITS (15-31)
constexpr static u32 VSM_PAGE_PHYSICAL_ADDRESS_BITS = 16u;
constexpr static u32 VSM_PAGE_PHYSICAL_ADDRESS_MASK = (1u << VSM_PAGE_PHYSICAL_ADDRESS_BITS) - 1u;

public struct VSMPageMetadata {
    public u32 data;

    [mutating]
    public __init(u32 data) {
        this.data = data;
    }

    public func is_visible() -> bool {
        return (this.data & VSM_PAGE_VISIBLE_BIT) != 0;
    }

    [mutating]
    public func set_visible(bool v) -> void {
        // first we need to clear already set bit, and set the actual value we want
        this.data = (this.data & ~VSM_PAGE_VISIBLE_BIT) | (VSM_PAGE_VISIBLE_BIT * u32(v));
    }

    public func is_dirty() -> bool {
        return (this.data & VSM_PAGE_DIRTY_BIT) != 0;
    }

    [mutating]
    public func set_dirty(bool v) -> void {
        this.data = (this.data & ~VSM_PAGE_DIRTY_BIT) | (VSM_PAGE_DIRTY_BIT * u32(v));
    }

    public func is_backed() -> bool {
        return (this.data & VSM_PAGE_BACKED_BIT) != 0;
    }

    [mutating]
    public func set_backed(bool v) -> void {
        this.data = (this.data & ~VSM_PAGE_BACKED_BIT) | (VSM_PAGE_BACKED_BIT * u32(v));
    }

    public func get_physical_address() -> u32 {
        return this.data >> VSM_PAGE_PHYSICAL_ADDRESS_BITS;
    }

    [mutating]
    public func set_physical_address(u32 new_physical_address) -> void {
        this.data = (this.data & VSM_PAGE_PHYSICAL_ADDRESS_MASK) | ((new_physical_address & VSM_PAGE_PHYSICAL_ADDRESS_MASK) << 16u);
    }
};