#pragma once

#include <atomic>

// TODO: We won't have this until we get atomic_ref (LLVM 19)

namespace lr {
template<typename T>
struct ManagedObj {
private:
    u64 ref_count = 0;
};
} // namespace lr
