#include "Stack.hh"

#include "Engine/OS/OS.hh"

namespace lr::memory {
ThreadStack::ThreadStack() {
    constexpr static usize stack_size = ls::mib_to_bytes(128);
    ptr = static_cast<u8 *>(mem_reserve(stack_size));
    mem_commit(ptr, stack_size);
}

ThreadStack::~ThreadStack() {
    mem_release(ptr);
}

ScopedStack::ScopedStack() {
    auto &stack = get_thread_stack();
    ptr = stack.ptr;
}

ScopedStack::~ScopedStack() {
    auto &stack = get_thread_stack();
    stack.ptr = ptr;
}

}  // namespace lr::memory
