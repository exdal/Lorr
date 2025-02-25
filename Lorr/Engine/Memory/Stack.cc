#include "Stack.hh"

#include "Engine/OS/OS.hh"

namespace lr::memory {
ThreadStack::ThreadStack() {
    ZoneScoped;

    constexpr static usize stack_size = ls::mib_to_bytes(32);
    ptr = static_cast<u8 *>(os::mem_reserve(stack_size));
    os::mem_commit(ptr, stack_size);
}

ThreadStack::~ThreadStack() {
    ZoneScoped;

    os::mem_release(ptr);
}

ScopedStack::ScopedStack() {
    ZoneScoped;

    auto &stack = get_thread_stack();
    ptr = stack.ptr;
}

ScopedStack::~ScopedStack() {
    ZoneScoped;

    auto &stack = get_thread_stack();
    stack.ptr = ptr;
}

}  // namespace lr::memory
