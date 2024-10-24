#include "Engine/Graphics/Vulkan/Impl.hh"

namespace lr {
inline u32 lzcnt_nonzero(u32 v) {
#ifdef _MSC_VER
    unsigned long retVal;
    _BitScanReverse(&retVal, v);
    return 31 - retVal;
#else
    return __builtin_clz(v);
#endif
}

inline u32 tzcnt_nonzero(u32 v) {
#ifdef _MSC_VER
    unsigned long retVal;
    _BitScanForward(&retVal, v);
    return retVal;
#else
    return __builtin_ctz(v);
#endif
}

namespace SmallFloat {
    static constexpr u32 MANTISSA_BITS = 3;
    static constexpr u32 MANTISSA_VALUE = 1 << MANTISSA_BITS;
    static constexpr u32 MANTISSA_MASK = MANTISSA_VALUE - 1;

    // Bin sizes follow floating point (exponent + mantissa) distribution (piecewise linear log approx)
    // This ensures that for each size class, the average overhead percentage stays the same
    u32 uintToFloatRoundUp(u32 size) {
        u32 exp = 0;
        u32 mantissa = 0;

        if (size < MANTISSA_VALUE) {
            // Denorm: 0..(MANTISSA_VALUE-1)
            mantissa = size;
        } else {
            // Normalized: Hidden high bit always 1. Not stored. Just like float.
            u32 leadingZeros = lzcnt_nonzero(size);
            u32 highestSetBit = 31 - leadingZeros;

            u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
            exp = mantissaStartBit + 1;
            mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;

            u32 lowBitsMask = (1 << mantissaStartBit) - 1;

            // Round up!
            if ((size & lowBitsMask) != 0)
                mantissa++;
        }

        return (exp << MANTISSA_BITS) + mantissa;  // + allows mantissa->exp overflow for round up
    }

    u32 uintToFloatRoundDown(u32 size) {
        u32 exp = 0;
        u32 mantissa = 0;

        if (size < MANTISSA_VALUE) {
            // Denorm: 0..(MANTISSA_VALUE-1)
            mantissa = size;
        } else {
            // Normalized: Hidden high bit always 1. Not stored. Just like float.
            u32 leadingZeros = lzcnt_nonzero(size);
            u32 highestSetBit = 31 - leadingZeros;

            u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
            exp = mantissaStartBit + 1;
            mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;
        }

        return (exp << MANTISSA_BITS) | mantissa;
    }

    u32 floatToUint(u32 floatValue) {
        u32 exponent = floatValue >> MANTISSA_BITS;
        u32 mantissa = floatValue & MANTISSA_MASK;
        if (exponent == 0) {
            // Denorms
            return mantissa;
        } else {
            return (mantissa | MANTISSA_VALUE) << (exponent - 1);
        }
    }
}  // namespace SmallFloat

ls::option<u32> findLowestSetBitAfter(u32 bitMask, u32 startBitIndex) {
    u32 maskBeforeStartIndex = (1 << startBitIndex) - 1;
    u32 maskAfterStartIndex = ~maskBeforeStartIndex;
    u32 bitsAfter = bitMask & maskAfterStartIndex;
    if (bitsAfter == 0)
        return ls::nullopt;
    return tzcnt_nonzero(bitsAfter);
}

auto TransferManager::create(Device_H device, u32 capacity, vk::BufferUsage additional_buffer_usage, u32 max_allocs) -> TransferManager {
    ZoneScoped;

    auto impl = new Impl;
    impl->device = device;
    impl->semaphore = Semaphore::create(device, 0).value();
    impl->buffer_id = Buffer::create(
                          device,
                          vk::BufferUsage::TransferSrc | additional_buffer_usage,
                          capacity,
                          vk::MemoryAllocationUsage::PreferHost,
                          vk::MemoryAllocationFlag::HostSeqWrite)
                          .value();
    impl->size = capacity;
    impl->max_allocs = max_allocs;
    impl->free_offset = max_allocs - 1;

    for (auto &used_bin : impl->used_bins) {
        used_bin = 0;
    }

    for (auto &bin_index : impl->bin_indices) {
        bin_index = ls::nullopt;
    }

    impl->nodes.resize(max_allocs);
    impl->free_nodes.resize(max_allocs);
    for (u32 i = 0; i < max_allocs; i++) {
        impl->free_nodes[i] = max_allocs - i - 1;
    }

    // fill storage
    impl->insert_node_into_bin(capacity, 0);

    return TransferManager(impl);
}

auto TransferManager::destroy() -> void {
}

auto TransferManager::allocate(u64 size, u64 alignment) -> ls::option<GPUAllocation> {
    ZoneScoped;

    auto aligned_size = ls::align_up(size, alignment);
    auto node_index = impl->find_free_node(static_cast<u32>(aligned_size));
    if (!node_index.has_value()) {
        return ls::nullopt;
    }

    impl->tracked_nodes.emplace_back(node_index.value(), impl->semaphore.value());

    auto &node = impl->nodes[node_index.value()];
    auto buffer_ptr = impl->device.buffer(impl->buffer_id).host_ptr();
    return GPUAllocation{
        .buffer_id = impl->buffer_id,
        .ptr = buffer_ptr + node.offset,
        .offset = node.offset,
        .size = node.size,
    };
}

auto TransferManager::collect_garbage() -> void {
    ZoneScoped;

    auto sema_counter = impl->semaphore.counter();
    for (auto &node : impl->tracked_nodes) {
        if (sema_counter > node.sema_value) {
            impl->free_node(node.resource);
            impl->tracked_nodes.pop_front();
            continue;
        }

        break;
    }
}

auto TransferManager::semaphore() const -> Semaphore {
    return impl->semaphore;
}

auto TransferManager::Impl::find_free_node(u32 node_size) -> ls::option<TransferManager::NodeID> {
    ZoneScoped;

    if (this->free_offset == 0) {
        return ls::nullopt;
    }

    auto min_bin_index = SmallFloat::uintToFloatRoundUp(node_size);
    u32 min_top_bin_index = min_bin_index >> TransferManager::TOP_BINS_INDEX_SHIFT;
    u32 min_leaf_bin_index = min_bin_index & TransferManager::LEAF_BINS_INDEX_MASK;
    ls::option<u32> top_bin_index = min_top_bin_index;
    ls::option<u32> leaf_bin_index = ls::nullopt;

    if (this->used_bins_top & (1 << top_bin_index.value())) {
        leaf_bin_index = findLowestSetBitAfter(this->used_bins[top_bin_index.value()], min_leaf_bin_index);
    }

    if (!leaf_bin_index.has_value()) {
        top_bin_index = findLowestSetBitAfter(this->used_bins_top, min_top_bin_index + 1);
        if (!top_bin_index.has_value()) {
            return ls::nullopt;
        }

        leaf_bin_index = tzcnt_nonzero(this->used_bins[top_bin_index.value()]);
    }

    auto bin_index = (top_bin_index.value() << TransferManager::TOP_BINS_INDEX_SHIFT) | leaf_bin_index.value();
    u32 node_index = this->bin_indices[bin_index].value();
    auto &node = this->nodes[node_index];
    auto node_total_size = node.size;
    node.size = node_size;
    node.used = true;

    this->bin_indices[bin_index] = node.next_bin;
    if (node.next_bin.has_value()) {
        this->nodes[node.next_bin.value()].prev_bin = ls::nullopt;
    }

    this->free_storage -= node_total_size;
    if (!this->bin_indices[bin_index].has_value()) {
        this->used_bins[top_bin_index.value()] &= ~(1 << leaf_bin_index.value());
        if (this->used_bins[top_bin_index.value()] == 0) {
            this->used_bins_top &= ~(1 << top_bin_index.value());
        }
    }

    u32 remainder_size = node_total_size - node_size;
    if (remainder_size > 0) {
        u32 new_node_index = this->insert_node_into_bin(remainder_size, node.offset + node_size);
        if (node.next_neighbor.has_value()) {
            this->nodes[node.next_neighbor.value()].prev_neighbor = new_node_index;
        }

        this->nodes[new_node_index].prev_neighbor = node_index;
        this->nodes[new_node_index].next_neighbor = node.next_neighbor;
        node.next_neighbor = new_node_index;
    }

    return node_index;
}

auto TransferManager::Impl::free_node(u32 node_index) -> void {
    ZoneScoped;

    auto &node = this->nodes[node_index];
    LS_EXPECT(node.used);

    u32 node_offset = node.offset;
    u32 node_size = node.size;

    // merge two nodes together
    if (node.prev_neighbor.has_value() && this->nodes[node.prev_neighbor.value()].used == false) {
        auto &prev_node = this->nodes[node.prev_neighbor.value()];
        node_offset = prev_node.offset;
        node_size += prev_node.size;

        this->remove_node_from_bin(node.prev_neighbor.value());

        LS_EXPECT(prev_node.next_neighbor.value() == node_index);
        node.prev_neighbor = prev_node.prev_neighbor;
    }

    if (node.next_neighbor.has_value() && this->nodes[node.next_neighbor.value()].used == false) {
        auto &next_node = this->nodes[node.next_neighbor.value()];
        node_size += next_node.size;

        this->remove_node_from_bin(node.next_neighbor.value());

        LS_EXPECT(next_node.prev_neighbor.value() == node_index);
        node.next_neighbor = next_node.next_neighbor;
    }

    auto next_neighbor = node.next_neighbor;
    auto prev_neighbor = node.prev_neighbor;
    this->free_nodes[++this->free_offset] = node_index;

    auto merged_node_index = this->insert_node_into_bin(node_size, node_offset);
    if (next_neighbor.has_value()) {
        this->nodes[merged_node_index].next_neighbor = next_neighbor;
        this->nodes[next_neighbor.value()].prev_neighbor = merged_node_index;
    }

    if (prev_neighbor.has_value()) {
        this->nodes[merged_node_index].prev_neighbor = prev_neighbor;
        this->nodes[prev_neighbor.value()].next_neighbor = merged_node_index;
    }
}

auto TransferManager::Impl::insert_node_into_bin(u32 node_size, u32 data_offset) -> u32 {
    ZoneScoped;

    auto bin_index = SmallFloat::uintToFloatRoundDown(node_size);
    u32 top_bin_index = bin_index >> TransferManager::TOP_BINS_INDEX_SHIFT;
    u32 leaf_bin_index = bin_index & TransferManager::LEAF_BINS_INDEX_MASK;

    if (!this->bin_indices[bin_index].has_value()) {
        this->used_bins[top_bin_index] |= 1 << leaf_bin_index;
        this->used_bins_top |= 1 << top_bin_index;
    }

    auto top_node_index = this->bin_indices[bin_index];
    auto node_index = this->free_nodes[this->free_offset--];

    this->nodes[node_index] = {
        .next_bin = top_node_index,
        .size = node_size,
        .offset = data_offset,
    };
    if (top_node_index.has_value()) {
        this->nodes[top_node_index.value()].prev_bin = node_index;
    }
    this->bin_indices[bin_index] = node_index;
    this->free_storage += node_size;

    return node_index;
}

auto TransferManager::Impl::remove_node_from_bin(u32 node_index) -> void {
    ZoneScoped;

    auto &node = this->nodes[node_index];
    if (node.prev_bin.has_value()) {
        // Not first node, merge prev and next together
        this->nodes[node.prev_bin.value()].next_bin = node.next_bin;
        if (node.next_bin.has_value()) {
            this->nodes[node.next_bin.value()].prev_bin = node.prev_bin;
        }
    } else {
        // First node
        auto bin_index = SmallFloat::uintToFloatRoundDown(node.size);
        u32 top_bin_index = bin_index >> TransferManager::TOP_BINS_INDEX_SHIFT;
        u32 leaf_bin_index = bin_index & TransferManager::LEAF_BINS_INDEX_MASK;

        this->bin_indices[bin_index] = node.next_bin;
        if (node.next_bin.has_value()) {
            this->nodes[node.next_bin.value()].prev_bin = ls::nullopt;
        }

        if (!this->bin_indices[bin_index].has_value()) {
            this->used_bins[top_bin_index] &= ~(1 << leaf_bin_index);
            if (this->used_bins[top_bin_index] == 0) {
                this->used_bins_top &= ~(1 << top_bin_index);
            }
        }
    }

    this->free_nodes[++this->free_offset] = node_index;
    this->free_storage -= node.size;
}

}  // namespace lr
