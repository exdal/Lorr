// Created on Friday November 18th 2022 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#include "TLSFAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
void TLSFAllocatorView::init(u64 mem_size, u32 max_allocs)
{
    m_max_size = mem_size;
    u32 firstIndex = get_first_index(mem_size);
    u32 blockCount = firstIndex * SL_INDEX_COUNT + get_second_index(firstIndex, mem_size);

    m_blocks.resize(blockCount);
    m_free_blocks.resize(max_allocs);
    m_block_indices.resize(blockCount);
    memset(m_block_indices.data(), TLSFBlock::k_invalid, sizeof(TLSFBlockID) * blockCount);

    for (u32 i = 0; i < max_allocs; i++)
        m_free_blocks[i] = max_allocs - i - 1;

    m_free_list_offset = max_allocs - 1;

    add_free_block(mem_size, 0);
}

TLSFBlockID TLSFAllocatorView::allocate(u64 size, u64 alignment)
{
    ZoneScoped;

    u64 aligned_size = align_up(size, alignment);
    TLSFBlockID found_block = find_free_block(aligned_size);
    if (found_block != TLSFBlock::k_invalid)
    {
        remove_free_block(found_block, false);

        u64 block_size = get_physical_size(found_block);
        u64 size_diff = block_size - aligned_size;
        if (size_diff != 0)
        {
            TLSFBlock &block = m_blocks[found_block];
            TLSFBlockID newBlockID = add_free_block(size_diff, block.m_offset + aligned_size);

            m_blocks[newBlockID].m_prev_physical = found_block;
            m_blocks[newBlockID].m_next_physical = block.m_next_physical;

            if (block.m_next_physical != TLSFBlock::k_invalid)
                m_blocks[block.m_next_physical].m_prev_physical = newBlockID;
            block.m_next_physical = newBlockID;
        }
    }

    return found_block;
}

void TLSFAllocatorView::free(TLSFBlockID blockID)
{
    ZoneScoped;

    TLSFBlock &block = m_blocks[blockID];

    u64 size = get_physical_size(blockID);
    u64 offset = block.m_offset;

    // Merge prev/next physical blocks together
    if (block.m_prev_physical != TLSFBlock::k_invalid && m_blocks[block.m_prev_physical].m_is_free)
    {
        TLSFBlock &prevBlock = m_blocks[block.m_prev_physical];
        offset = prevBlock.m_offset;
        size += get_physical_size(block.m_prev_physical);

        remove_free_block(block.m_prev_physical);
        block.m_prev_physical = prevBlock.m_prev_physical;
    }

    if (block.m_next_physical != TLSFBlock::k_invalid && m_blocks[block.m_next_physical].m_is_free)
    {
        TLSFBlock &nextBlock = m_blocks[block.m_next_physical];
        size += get_physical_size(block.m_next_physical);

        remove_free_block(block.m_next_physical);
        block.m_next_physical = nextBlock.m_next_physical;
    }

    m_free_blocks[++m_free_list_offset] = blockID;

    TLSFBlockID newBlockID = add_free_block(size, offset);
    TLSFBlock &newBlock = m_blocks[newBlockID];

    if (block.m_prev_physical != TLSFBlock::k_invalid)
    {
        newBlock.m_prev_physical = block.m_prev_physical;
        m_blocks[block.m_prev_physical].m_next_physical = newBlockID;
    }

    if (block.m_next_physical != TLSFBlock::k_invalid)
    {
        newBlock.m_next_physical = block.m_next_physical;
        m_blocks[block.m_next_physical].m_prev_physical = newBlockID;
    }
}

TLSFBlockID TLSFAllocatorView::find_free_block(u64 size)
{
    ZoneScoped;

    u32 firstIndex = get_first_index(size);
    u32 secondIndex = get_second_index(firstIndex, size);

    u32 secondListMap = m_second_list_bitmap[firstIndex] & (~0U << secondIndex);
    if (secondListMap == 0)
    {
        // Could not find any available block in that index, so go higher
        u32 firstListMap = m_first_list_bitmap & (~0U << (firstIndex + 1));
        if (firstListMap == 0)
        {
            return TLSFBlock::k_invalid;
        }

        firstIndex = find_lsb(firstListMap);
        secondListMap = m_second_list_bitmap[firstIndex];
    }

    secondIndex = find_lsb(secondListMap);

    return m_block_indices[get_free_list_index(firstIndex, secondIndex)];
}

TLSFBlockID TLSFAllocatorView::add_free_block(u64 size, u64 offset)
{
    ZoneScoped;

    u32 firstIndex = get_first_index(size);
    u32 secondIndex = get_second_index(firstIndex, size);
    u32 freeListIdx = get_free_list_index(firstIndex, secondIndex);
    TLSFBlockID freeListBlockID = m_block_indices[freeListIdx];
    TLSFBlockID newBlockID = m_free_blocks[m_free_list_offset--];
    TLSFBlock &block = m_blocks[newBlockID];

    block.m_offset = offset;
    block.m_prev_free = TLSFBlock::k_invalid;
    block.m_next_free = freeListBlockID;
    block.m_is_free = true;

    if (freeListBlockID != TLSFBlock::k_invalid)
        m_blocks[freeListBlockID].m_prev_free = newBlockID;
    else
    {
        m_first_list_bitmap |= 1 << firstIndex;
        m_second_list_bitmap[firstIndex] |= 1 << secondIndex;
    }

    m_block_indices[freeListIdx] = newBlockID;

    return newBlockID;
}

void TLSFAllocatorView::remove_free_block(TLSFBlockID blockID, bool removeBlock)
{
    ZoneScoped;

    TLSFBlock &block = m_blocks[blockID];
    u64 blockSize = get_physical_size(blockID);

    if (block.m_prev_free != TLSFBlock::k_invalid)
    {
        // This is head of the chain, since we already know it, no need to calculate entire freelist index

        m_blocks[block.m_prev_free].m_next_free = block.m_next_free;
        if (block.m_next_free != TLSFBlock::k_invalid)
            m_blocks[block.m_next_free].m_prev_free = block.m_prev_free;
    }
    else
    {
        u32 firstIndex = get_first_index(blockSize);
        u32 secondIndex = get_second_index(firstIndex, blockSize);
        u32 freeListIndex = get_free_list_index(firstIndex, secondIndex);

        m_block_indices[freeListIndex] = block.m_next_free;

        if (block.m_next_free != TLSFBlock::k_invalid)
            m_blocks[block.m_next_free].m_prev_free = TLSFBlock::k_invalid;
        else
        {
            m_second_list_bitmap[firstIndex] &= ~(1 << secondIndex);
            if (m_second_list_bitmap[firstIndex] == 0)
            {
                m_first_list_bitmap &= ~(1 << firstIndex);
            }
        }
    }

    block.m_is_free = false;

    if (removeBlock)
        m_free_blocks[++m_free_list_offset] = blockID;
}

u32 TLSFAllocatorView::get_first_index(u64 size)
{
    ZoneScoped;

    if (size < MIN_BLOCK_SIZE)
        return 0;

    return find_msb(size) - (FL_INDEX_SHIFT - 1);
}

u32 TLSFAllocatorView::get_second_index(u32 first_index, u64 size)
{
    ZoneScoped;

    if (size < MIN_BLOCK_SIZE)
        return size / (MIN_BLOCK_SIZE / SL_INDEX_COUNT);

    return (size >> ((first_index + (FL_INDEX_SHIFT - 1)) - SL_INDEX_COUNT_LOG2)) ^ SL_INDEX_COUNT;
}

u32 TLSFAllocatorView::get_free_list_index(u32 first_index, u32 second_index)
{
    ZoneScoped;

    if (first_index == 0)
        return second_index;

    return (first_index * SL_INDEX_COUNT + second_index) - 1;
}

u64 TLSFAllocatorView::get_physical_size(TLSFBlockID block_id)
{
    ZoneScoped;

    assert(block_id != TLSFBlock::k_invalid);
    TLSFBlock &block = m_blocks[block_id];

    if (block.m_next_physical != TLSFBlock::k_invalid)
        return m_blocks[block.m_next_physical].m_offset - block.m_offset;

    return m_max_size - block.m_offset;
}

TLSFBlock *TLSFAllocatorView::get_block_data(TLSFBlockID block_id)
{
    ZoneScoped;

    assert(block_id != TLSFBlock::k_invalid);

    return &m_blocks[block_id];
}

void TLSFAllocator::init(u64 mem_size, u32 max_allocs)
{
    m_view.init(mem_size, max_allocs);
    m_data = Memory::Allocate<u8>(mem_size);
}

TLSFAllocator::~TLSFAllocator()
{
    Memory::Release(m_data);
}

bool TLSFAllocator::can_allocate(u64 size, u32 alignment)
{
    ZoneScoped;

    return false;  // TODO
}

void *TLSFAllocator::allocate(TLSFBlockID &blockIDOut, u64 size, u32 alignment)
{
    ZoneScoped;

    blockIDOut = m_view.allocate(size, alignment);
    if (blockIDOut == TLSFBlock::k_invalid)
        return nullptr;

    return m_data + m_view.get_block_data(blockIDOut)->m_offset;
}

void TLSFAllocator::free(TLSFBlockID block_id, bool free_data)
{
    ZoneScoped;

    m_view.free(block_id);
    if (free_data)
        Memory::Release(m_data);
}

}  // namespace lr::Memory