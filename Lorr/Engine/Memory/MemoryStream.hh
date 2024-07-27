#pragma once

#include "Engine/OS/OS.hh"

namespace lr {
struct MemoryBlock {
    MemoryBlock *prev = nullptr;
    MemoryBlock *next = nullptr;
    usize offset = 0;
    usize size = 0;
    u8 *data = nullptr;
};

struct MemoryWriteStream;
template<typename T>
concept MemoryStreamWritable = requires(T &v, MemoryWriteStream &s) { T::_stream_write(v, s); };

struct MemoryWriteStream {
    MemoryBlock *head_mem_block = nullptr;
    MemoryBlock *mem_block = nullptr;
    usize min_block_size = 4096;

    MemoryWriteStream() { mem_block = alloc_block(min_block_size); }
    ~MemoryWriteStream() { reset(true); }

    template<typename T>
        requires MemoryStreamWritable<T>
    void write(T &v) {
        ZoneScoped;

        T::_stream_write(v, *this);
    }

    template<typename T>
    void write(const T &v) {
        ZoneScoped;

        auto block = get_block(sizeof(T));
        std::memcpy(block->data + block->offset, &v, sizeof(T));
        block->offset += sizeof(T);
    }

    void write(const void *data, usize size) {
        ZoneScoped;

        auto block = get_block(size);
        std::memcpy(block->data + block->offset, data, size);
        block->offset += size;
    }

    void write_str8(std::string_view str) {
        ZoneScoped;

        write(static_cast<u8>(str.length()));
        write(str.data(), str.length());
    }

    void write_str16(std::string_view str) {
        ZoneScoped;

        write(static_cast<u16>(str.length()));
        write(str.data(), str.length());
    }

    void write_str(std::string_view str) {
        ZoneScoped;

        write(static_cast<u32>(str.length()));
        write(str.data(), str.length());
    }

    void reset(this MemoryWriteStream &self, bool free = false) {
        MemoryBlock *cur_block = self.mem_block;
        while (cur_block != nullptr) {
            cur_block->offset = 0;

            if (free) {
                MemoryBlock *tmp = cur_block;
                cur_block = cur_block->prev;

                delete tmp;
                tmp = nullptr;

                continue;
            }

            cur_block = cur_block->prev;
        }

        self.mem_block = self.head_mem_block;
    }

    void to_file(this MemoryWriteStream &self, os::File file) {
        ZoneScoped;

        MemoryBlock *mem_block = self.head_mem_block;
        while (mem_block != nullptr) {
            os::write_file(file, mem_block->data, { 0, mem_block->offset });
            mem_block = mem_block->next;
        };
    }

private:
    MemoryBlock *alloc_block(this MemoryWriteStream &self, usize size) {
        ZoneScoped;

        usize block_size = sizeof(MemoryBlock) + size;
        u8 *data = new u8[block_size];
        MemoryBlock *block = new (data) MemoryBlock{};
        block->data = new (data + sizeof(MemoryBlock)) u8[size];

        if (!self.head_mem_block) {
            self.head_mem_block = block;
        }

        if (self.mem_block) {
            self.mem_block->next = block;
        }

        block->prev = self.mem_block;
        block->size = size;

        return block;
    }

    MemoryBlock *get_block(this MemoryWriteStream &self, usize size) {
        ZoneScoped;

        auto block = self.mem_block;
        usize remainder = block->size - block->offset;
        if (remainder >= size) {
            return block;
        }

        block = self.alloc_block(size);
        self.mem_block = block;

        return block;
    }
};
}  // namespace lr
