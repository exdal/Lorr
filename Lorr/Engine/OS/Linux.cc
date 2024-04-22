#include "OS.hh"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>

namespace lr {
u64 os::page_size()
{
    return 0;
}

void *os::reserve(u64 size)
{
    return mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void os::release(void *data, u64 size)
{
    munmap(data, size);
}

bool os::commit(void *data, u64 size)
{
    return mprotect(data, size, PROT_READ | PROT_WRITE);
}

void os::decommit(void *data, u64 size)
{
    madvise(data, size, MADV_DONTNEED);
    mprotect(data, size, PROT_NONE);
}
}  // namespace lr
