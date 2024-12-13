#pragma once

#include <atomic>

namespace lr {
template<typename T>
struct ManagedObj {
private:
    u64 ref_count = 0;
};
}  // namespace lr
