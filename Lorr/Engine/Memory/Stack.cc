#include "Stack.hh"

#include "OS/OS.hh"

namespace lr::memory {
ThreadStack::ThreadStack()
{
    constexpr static usize stack_size = mib_to_bytes(128);
    ptr = static_cast<u8 *>(os::reserve(stack_size));
    os::commit(ptr, stack_size);
}

ThreadStack::~ThreadStack()
{
    os::release(ptr);
}

ScopedStack::ScopedStack()
{
    auto &stack = GetThreadStack();
    ptr = stack.ptr;
}

ScopedStack::~ScopedStack()
{
    auto &stack = GetThreadStack();
    stack.ptr = ptr;
}

}  // namespace lr::memory
