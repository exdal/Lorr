#pragma once

namespace lr {
template<typename T, i32 SIZE>
struct ScrollingBuffer {
    i32 size = SIZE;
    bool wrapped = {};
    i32 offset = {};
    std::array<T, SIZE> data = {};

    ScrollingBuffer() = default;
    void add_point(T point) {
        data[offset] = point;
        wrapped |= (offset + 1) == size;
        offset = (offset + 1) % size;
    }
    
    void erase() {
        data.fill(T());
        wrapped = false;
        offset = 0;
    }
    
    auto back() {
        i32 back_index = static_cast<i32>(glm::mod(static_cast<f32>(offset - 1), static_cast<f32>(size)));
        return data[back_index];
    }

    auto front() {
        if (wrapped) {
            return data[(offset)];
        } else {
            return data[0];
        }
    }
};
} // namespace lr
