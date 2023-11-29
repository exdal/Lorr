#pragma once

namespace lr::Memory
{
using TLSFBlockID = u32;
struct TLSFBlock
{
    static constexpr TLSFBlockID k_invalid = ~0;

    u64 m_offset : 63 = ~0;
    u64 m_is_free : 1 = false;

    TLSFBlockID m_next_physical = k_invalid;
    TLSFBlockID m_prev_physical = k_invalid;

    TLSFBlockID m_prev_free = k_invalid;
    TLSFBlockID m_next_free = k_invalid;
};

struct TLSFAllocatorView
{
    static constexpr i32 ALIGN_SIZE_LOG2 = 3;
    static constexpr i32 ALIGN_SIZE = 1 << ALIGN_SIZE_LOG2;

    static constexpr i32 SL_INDEX_COUNT_LOG2 = 5;
    static constexpr i32 SL_INDEX_COUNT = 1 << SL_INDEX_COUNT_LOG2;

    static constexpr i32 FL_INDEX_MAX = 32;  // support up to 4 GB of allocation
    static constexpr i32 FL_INDEX_SHIFT = SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2;
    static constexpr i32 FL_INDEX_COUNT = FL_INDEX_MAX - FL_INDEX_SHIFT + 1;
    static constexpr i32 FL_INDEX_SHIFT_SIZE = 1 << FL_INDEX_SHIFT;

    static constexpr u64 MIN_BLOCK_SIZE = 1 << FL_INDEX_SHIFT;

    /// ------------------------------------------------------------ ///

    void init(u64 mem_size, u32 max_allocs);
    ~TLSFAllocatorView();

    TLSFBlockID allocate(u64 size, u64 alignment = ALIGN_SIZE);
    void free(TLSFBlockID blockID);

    TLSFBlockID find_free_block(u64 size);
    TLSFBlockID add_free_block(u64 size, u64 offset);
    void remove_free_block(TLSFBlockID blockID, bool removeBlock = true);

    // Bitmap utils
    u32 get_first_index(u64 size);
    u32 get_second_index(u32 first_index, u64 size);
    u32 get_free_list_index(u32 first_index, u32 second_index);
    u64 get_physical_size(TLSFBlockID block_id);
    TLSFBlock *get_block_data(TLSFBlockID block_id);

    u32 m_first_list_bitmap = 0;
    u32 m_second_list_bitmap[FL_INDEX_COUNT] = {};

    TLSFBlock *m_blocks = nullptr;
    TLSFBlockID *m_block_indices = nullptr;
    TLSFBlockID *m_free_blocks = nullptr;
    u32 m_free_list_offset = 0;

    u64 m_max_size = 0;
    u64 m_used_size = 0;
};

struct TLSFAllocator
{
    void init(u64 mem_size, u32 max_allocs);
    ~TLSFAllocator();
    bool can_allocate(u64 size, u32 alignment = 1);
    void *allocate(TLSFBlockID &blockIDOut, u64 size, u32 alignment = 1);
    void free(TLSFBlockID block_id, bool free_data);

    TLSFAllocatorView m_view;
    u8 *m_data = nullptr;
};
}  // namespace lr::Memory