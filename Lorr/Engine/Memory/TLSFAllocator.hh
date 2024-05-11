#pragma once

#include "Memory/Bit.hh"

namespace lr::memory {
template<bool IS_64_BIT = true>
struct TLSFAllocator {
    // Note on ALIGN_#: we can remove them and just set FL_INDEX_SHIFT to SL_COUNT when we
    // do gpu mem management
    constexpr static usize SL_INDEX_COUNT = 5u;
    constexpr static usize SL_INDEX_SIZE = 1 << SL_INDEX_COUNT;
    constexpr static usize FL_INDEX_MAX = IS_64_BIT ? 32u : 30u;
    constexpr static usize FL_INDEX_SHIFT = SL_INDEX_COUNT + 3;
    constexpr static usize FL_INDEX_COUNT = FL_INDEX_MAX - FL_INDEX_SHIFT + 1;
    constexpr static usize MIN_BLOCK_SIZE = 1 << FL_INDEX_SHIFT;

    using BlockID = u32;
    struct MemoryBlock {
        constexpr static BlockID INVALID = ~0u;
        BlockID next_physical = INVALID;
        BlockID prev_physical = INVALID;
        BlockID next_logical = INVALID;
        BlockID prev_logical = INVALID;

        u64 offset : 63 = ~0ULL;
        bool is_free : 1 = false;
    };

    TLSFAllocator() = default;
    TLSFAllocator(u64 mem_size, u64 max_allocs)
    {
        auto [fi, si] = get_indexes(mem_size);
        usize block_count = get_block_index(fi, si) + 1;

        m_capacity = mem_size;
        m_blocks.resize(max_allocs);
        m_block_ids.resize(block_count);
        std::memset(m_block_ids.data(), MemoryBlock::INVALID, sizeof(BlockID) * block_count);

        m_free_block_ids.resize(max_allocs);
        for (u32 i = 0; i < max_allocs; i++) {
            m_free_block_ids[i] = max_allocs - i - 1;
        }

        m_last_free_list_id = max_allocs - 1;
        add_free_block(mem_size, 0);
    }

    BlockID allocate(usize size, usize alignment = alignof(double))
    {
        usize aligned_size = align_up(size, alignment);
        BlockID block_id = find_free_block(aligned_size);

        if (block_id != MemoryBlock::INVALID) {
            BlockID split_block = split_free_block(block_id, aligned_size);
            if (split_block != MemoryBlock::INVALID) {
                m_blocks[block_id].next_physical = split_block;
            }
        }

        return block_id;
    }

    void free(BlockID block_id)
    {
        MemoryBlock &block = m_blocks[block_id];
        u64 block_size = get_physical_size(block_id);
        u64 block_offset = block.offset;

        // Merge 2 neighbor physical blocks together
        if (block.prev_physical != MemoryBlock::INVALID && m_blocks[block.prev_physical].is_free) {
            MemoryBlock &prev_block = m_blocks[block.prev_physical];

            block_offset = prev_block.offset;
            block_size += get_physical_size(block.prev_physical);

            remove_free_block(block.prev_physical);
            block.prev_physical = prev_block.prev_physical;
        }

        if (block.next_physical != MemoryBlock::INVALID && m_blocks[block.next_physical].is_free) {
            MemoryBlock &next_block = m_blocks[block.next_physical];

            block_size += get_physical_size(block.next_physical);

            remove_free_block(block.next_physical);
            block.next_physical = next_block.next_physical;
        }

        // Create a new block, relative to removed 2 blocks
        m_free_block_ids[++m_last_free_list_id] = block_id;
        BlockID new_block_id = add_free_block(block_size, block_offset);
        MemoryBlock &new_block = m_blocks[new_block_id];
        new_block.prev_physical = block.prev_physical;
        new_block.next_physical = block.next_physical;

        if (block.prev_physical != MemoryBlock::INVALID) {
            m_blocks[block.prev_physical].next_physical = new_block_id;
        }

        if (block.next_physical != MemoryBlock::INVALID) {
            m_blocks[block.next_physical].prev_physical = new_block_id;
        }
    }

    BlockID find_free_block(u64 size)
    {
        auto [fi, si] = get_indexes(size);
        u32 second_list = m_second_list_mask[fi] & (~0U << si);
        if (second_list == 0) {
            u32 first_list = m_first_list_mask & (~0U << (fi + 1));
            if (first_list == 0) {
                return MemoryBlock::INVALID;
            }

            fi = find_first_set64(first_list);
            second_list = m_second_list_mask[fi];
        }

        si = find_first_set64(second_list);
        return m_block_ids[get_block_index(fi, si)];
    }

    BlockID add_free_block(u64 size, u64 offset)
    {
        auto [fi, si] = get_indexes(size);
        u32 cur_block_index = get_block_index(fi, si);

        BlockID cur_block_id = m_block_ids[cur_block_index];
        BlockID new_block_id = m_free_block_ids[m_last_free_list_id--];
        MemoryBlock &new_block = m_blocks[new_block_id];

        new_block.offset = offset;
        new_block.prev_logical = MemoryBlock::INVALID;
        new_block.next_logical = cur_block_id;
        new_block.is_free = true;

        if (cur_block_id == MemoryBlock::INVALID) {
            m_first_list_mask |= 1 << fi;
            m_second_list_mask[fi] |= 1 << si;
        }
        else {
            m_blocks[cur_block_id].prev_logical = new_block_id;
        }

        m_block_ids[cur_block_index] = new_block_id;
        return new_block_id;
    }

    void remove_free_block(BlockID block_id)
    {
        MemoryBlock &block = m_blocks[block_id];
        u64 block_size = get_physical_size(block_id);

        // is this head block?
        if (block.prev_logical == MemoryBlock::INVALID) {
            auto [fi, si] = get_indexes(block_size);
            u32 cur_block_index = get_block_index(fi, si);

            // assign new head for second list
            m_block_ids[cur_block_index] = block.next_logical;

            if (block.next_logical != MemoryBlock::INVALID) {
                m_blocks[block.next_logical].prev_logical = MemoryBlock::INVALID;
            }
            else {
                m_second_list_mask[fi] &= ~(1 << si);
                if (m_second_list_mask[fi] == 0) {
                    m_first_list_mask &= ~(1 << fi);
                }
            }
        }
        else {
            m_blocks[block.prev_logical].next_logical = block.next_logical;
            if (block.next_logical != MemoryBlock::INVALID) {
                m_blocks[block.next_logical].prev_logical = block.prev_logical;
            }
        }

        block.is_free = false;
        m_free_block_ids[++m_last_free_list_id] = block_id;
    }

    BlockID split_free_block(BlockID block_id, u64 split_size)
    {
        u64 block_size = get_physical_size(block_id);
        remove_free_block(block_id);
        m_free_block_ids[m_last_free_list_id--] = block_id;

        u64 size_diff = block_size - split_size;
        if (size_diff > 0) {
            MemoryBlock &block = m_blocks[block_id];
            BlockID tailing_block_id = add_free_block(size_diff, block.offset + split_size);
            MemoryBlock &tailing_block = m_blocks[tailing_block_id];

            tailing_block.prev_physical = block_id;
            tailing_block.next_physical = block.next_physical;
            if (block.next_physical != MemoryBlock::INVALID) {
                m_blocks[block.next_physical].prev_physical = tailing_block_id;
            }

            return tailing_block_id;
        }

        return MemoryBlock::INVALID;
    }

    std::pair<u32, u32> get_indexes(u64 size)
    {
        if (size < MIN_BLOCK_SIZE)
            return { 0, size / (MIN_BLOCK_SIZE / SL_INDEX_SIZE) };

        u32 fi = find_least_set64(size);
        u32 si = (size >> (fi - SL_INDEX_COUNT)) ^ SL_INDEX_SIZE;
        return { fi - FL_INDEX_SHIFT - 1, si };
    }

    usize get_block_index(u32 fi, u32 si)
    {
        if (fi == 0)
            return si;

        return (fi * SL_INDEX_SIZE + si) - 1;
    }

    usize get_physical_size(BlockID block_id)
    {
        MemoryBlock &block = m_blocks[block_id];
        if (block.next_physical != MemoryBlock::INVALID) {
            return m_blocks[block.next_physical].offset - block.offset;
        }

        return m_capacity - block.offset;
    }

    MemoryBlock &get_block_data(BlockID block_id) { return m_blocks[block_id]; }

    u32 m_first_list_mask = 0;
    u32 m_second_list_mask[FL_INDEX_COUNT] = {};

    std::vector<MemoryBlock> m_blocks = {};
    std::vector<BlockID> m_block_ids = {};  // LOGICAL FREE-LIST indexes, dont confuse them
    std::vector<BlockID> m_free_block_ids = {};

    usize m_last_free_list_id = 0;
    usize m_capacity = 0;
};

}  // namespace lr::memory
