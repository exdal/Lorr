// Created on Friday November 18th 2022 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#include "TLSFAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
void TLSFAllocatorView::Init(u64 memSize, u32 maxAllocs)
{
    ZoneScoped;

    m_MaxSize = memSize;
    u32 firstIndex = GetFirstIndex(memSize);
    u32 blockCount = firstIndex * SL_INDEX_COUNT + GetSecondIndex(firstIndex, memSize);

    m_pBlocks = new TLSFBlock[maxAllocs];
    m_pFreeBlocks = new TLSFBlockID[maxAllocs];
    m_pBlockIndices = new TLSFBlockID[blockCount];
    memset(m_pBlockIndices, TLSFBlock::kInvalid, sizeof(TLSFBlockID) * blockCount);

    for (u32 i = 0; i < maxAllocs; i++)
        m_pFreeBlocks[i] = maxAllocs - i - 1;

    m_FreeListOffset = maxAllocs - 1;

    AddFreeBlock(memSize, 0);
}

void TLSFAllocatorView::Delete()
{
    ZoneScoped;
    
    delete[] m_pFreeBlocks;
    delete[] m_pBlocks;
    delete[] m_pBlockIndices;
}

TLSFBlockID TLSFAllocatorView::Allocate(u64 size, u64 alignment)
{
    ZoneScoped;
    
    TLSFBlockID foundBlock = FindFreeBlock(size);
    if (foundBlock != TLSFBlock::kInvalid)
    {
        RemoveFreeBlock(foundBlock, false);

        u64 blockSize = GetPhysicalSize(foundBlock);
        u64 sizeDiff = blockSize - size;
        if (sizeDiff != 0)
        {
            TLSFBlock &block = m_pBlocks[foundBlock];
            TLSFBlockID newBlockID = AddFreeBlock(sizeDiff, block.m_Offset + size);

            m_pBlocks[newBlockID].m_PrevPhysical = foundBlock;
            m_pBlocks[newBlockID].m_NextPhysical = block.m_NextPhysical;

            if (block.m_NextPhysical != TLSFBlock::kInvalid)
                m_pBlocks[block.m_NextPhysical].m_PrevPhysical = newBlockID;
            block.m_NextPhysical = newBlockID;
        }
    }

    return foundBlock;
}

void TLSFAllocatorView::Free(TLSFBlockID blockID)
{
    ZoneScoped;
    
    TLSFBlock &block = m_pBlocks[blockID];

    u64 size = GetPhysicalSize(blockID);
    u64 offset = block.m_Offset;

    // Merge prev/next physical blocks together
    if (block.m_PrevPhysical != TLSFBlock::kInvalid && m_pBlocks[block.m_PrevPhysical].m_IsFree)
    {
        TLSFBlock &prevBlock = m_pBlocks[block.m_PrevPhysical];
        offset = prevBlock.m_Offset;
        size += GetPhysicalSize(block.m_PrevPhysical);

        RemoveFreeBlock(block.m_PrevPhysical);
        block.m_PrevPhysical = prevBlock.m_PrevPhysical;
    }

    if (block.m_NextPhysical != TLSFBlock::kInvalid && m_pBlocks[block.m_NextPhysical].m_IsFree)
    {
        TLSFBlock &nextBlock = m_pBlocks[block.m_NextPhysical];
        size += GetPhysicalSize(block.m_NextPhysical);

        RemoveFreeBlock(block.m_NextPhysical);
        block.m_NextPhysical = nextBlock.m_NextPhysical;
    }

    m_pFreeBlocks[++m_FreeListOffset] = blockID;

    TLSFBlockID newBlockID = AddFreeBlock(size, offset);
    TLSFBlock &newBlock = m_pBlocks[newBlockID];

    if (block.m_PrevPhysical != TLSFBlock::kInvalid)
    {
        newBlock.m_PrevPhysical = block.m_PrevPhysical;
        m_pBlocks[block.m_PrevPhysical].m_NextPhysical = newBlockID;
    }

    if (block.m_NextPhysical != TLSFBlock::kInvalid)
    {
        newBlock.m_NextPhysical = block.m_NextPhysical;
        m_pBlocks[block.m_NextPhysical].m_PrevPhysical = newBlockID;
    }
}

TLSFBlockID TLSFAllocatorView::FindFreeBlock(u64 size)
{
    ZoneScoped;
    
    u32 firstIndex = GetFirstIndex(size);
    u32 secondIndex = GetSecondIndex(firstIndex, size);

    u32 secondListMap = m_pSecondListBitmap[firstIndex] & (~0U << secondIndex);
    if (secondListMap == 0)
    {
        // Could not find any available block in that index, so go higher
        u32 firstListMap = m_FirstListBitmap & (~0U << (firstIndex + 1));
        if (firstListMap == 0)
        {
            return TLSFBlock::kInvalid;
        }

        firstIndex = GetLSB(firstListMap);
        secondListMap = m_pSecondListBitmap[firstIndex];
    }

    secondIndex = GetLSB(secondListMap);

    return m_pBlockIndices[GetFreeListIndex(firstIndex, secondIndex)];
}

TLSFBlockID TLSFAllocatorView::AddFreeBlock(u64 size, u64 offset)
{
    ZoneScoped;
    
    u32 firstIndex = GetFirstIndex(size);
    u32 secondIndex = GetSecondIndex(firstIndex, size);
    u32 freeListIdx = GetFreeListIndex(firstIndex, secondIndex);
    TLSFBlockID freeListBlockID = m_pBlockIndices[freeListIdx];
    TLSFBlockID newBlockID = m_pFreeBlocks[m_FreeListOffset--];
    TLSFBlock &block = m_pBlocks[newBlockID];

    block.m_Offset = offset;
    block.m_PrevFree = TLSFBlock::kInvalid;
    block.m_NextFree = freeListBlockID;
    block.m_IsFree = true;

    if (freeListBlockID != TLSFBlock::kInvalid)
        m_pBlocks[freeListBlockID].m_PrevFree = newBlockID;
    else
    {
        m_FirstListBitmap |= 1 << firstIndex;
        m_pSecondListBitmap[firstIndex] |= 1 << secondIndex;
    }

    m_pBlockIndices[freeListIdx] = newBlockID;

    return newBlockID;
}

void TLSFAllocatorView::RemoveFreeBlock(TLSFBlockID blockID, bool removeBlock)
{
    ZoneScoped;
    
    TLSFBlock &block = m_pBlocks[blockID];
    u64 blockSize = GetPhysicalSize(blockID);

    if (block.m_PrevFree != TLSFBlock::kInvalid)
    {
        // This is head of the chain, since we already know it, no need to calculate entire freelist index

        m_pBlocks[block.m_PrevFree].m_NextFree = block.m_NextFree;
        if (block.m_NextFree != TLSFBlock::kInvalid)
            m_pBlocks[block.m_NextFree].m_PrevFree = block.m_PrevFree;
    }
    else
    {
        u32 firstIndex = GetFirstIndex(blockSize);
        u32 secondIndex = GetSecondIndex(firstIndex, blockSize);
        u32 freeListIndex = GetFreeListIndex(firstIndex, secondIndex);

        m_pBlockIndices[freeListIndex] = block.m_NextFree;

        if (block.m_NextFree != TLSFBlock::kInvalid)
            m_pBlocks[block.m_NextFree].m_PrevFree = TLSFBlock::kInvalid;
        else
        {
            m_pSecondListBitmap[firstIndex] &= ~(1 << secondIndex);
            if (m_pSecondListBitmap[firstIndex] == 0)
            {
                m_FirstListBitmap &= ~(1 << firstIndex);
            }
        }
    }

    block.m_IsFree = false;

    if (removeBlock)
        m_pFreeBlocks[++m_FreeListOffset] = blockID;
}

u32 TLSFAllocatorView::GetFirstIndex(u64 size)
{
    ZoneScoped;
    
    if (size < MIN_BLOCK_SIZE)
        return 0;

    return GetMSB(size) - (FL_INDEX_SHIFT - 1);
}

u32 TLSFAllocatorView::GetSecondIndex(u32 firstIndex, u64 size)
{
    ZoneScoped;
    
    if (size < MIN_BLOCK_SIZE)
        return size / (MIN_BLOCK_SIZE / SL_INDEX_COUNT);

    return (size >> ((firstIndex + (FL_INDEX_SHIFT - 1)) - SL_INDEX_COUNT_LOG2)) ^ SL_INDEX_COUNT;
}

u32 TLSFAllocatorView::GetFreeListIndex(u32 firstIndex, u32 secondIndex)
{
    ZoneScoped;
    
    if (firstIndex == 0)
        return secondIndex;

    return (firstIndex * SL_INDEX_COUNT + secondIndex) - 1;
}

u64 TLSFAllocatorView::GetPhysicalSize(TLSFBlockID blockID)
{
    ZoneScoped;
    
    assert(blockID != TLSFBlock::kInvalid);
    TLSFBlock &block = m_pBlocks[blockID];

    if (block.m_NextPhysical != TLSFBlock::kInvalid)
        return m_pBlocks[block.m_NextPhysical].m_Offset - block.m_Offset;
    else
        return m_MaxSize - block.m_Offset;
}

TLSFBlock *TLSFAllocatorView::GetBlockData(TLSFBlockID blockID)
{
    ZoneScoped;
    
    assert(blockID != TLSFBlock::kInvalid);

    return &m_pBlocks[blockID];
}

void TLSFAllocator::Init(const TLSFAllocatorDesc &desc)
{
    ZoneScoped;

    m_View.Init(desc.m_DataSize, desc.m_BlockCount);
    m_pData = Memory::Allocate<u8>(desc.m_DataSize);
}

void TLSFAllocator::Delete()
{
    ZoneScoped;

    m_View.Delete();
    Memory::Release(m_pData);
}

bool TLSFAllocator::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    return false;  // TODO
}

void *TLSFAllocator::Allocate(u64 size, u32 alignment, void **ppAllocatorData)
{
    ZoneScoped;

    TLSFBlockID blockID = m_View.Allocate(size, alignment);
    if (blockID == TLSFBlock::kInvalid)
        return nullptr;

    *ppAllocatorData = &blockID;
    return m_pData + m_View.GetBlockData(blockID)->m_Offset;
}

void TLSFAllocator::Free(TLSFBlockID blockID, bool freeData)
{
    ZoneScoped;

    m_View.Free(blockID);
    if (freeData)
        Memory::Release(m_pData);
}

}  // namespace lr::Memory