#include "OS.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if LR_LINUX
// Basic type declaration to shut LSP up.
using DWORD = u32;
using HANDLE = void *;
struct OVERLAPPED {
    usize Offset = 0;
    usize OffsetHigh = 0;
};

struct LARGE_INTEGER {
    usize QuadPart = 0;
};

enum : DWORD {
    OPEN_EXISTING,
    GENERIC_READ,
    FILE_SHARE_READ,
    GENERIC_WRITE,
    FILE_SHARE_WRITE,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    FILE_BEGIN,
};
static HANDLE INVALID_HANDLE_VALUE = nullptr;

HANDLE CreateFileW(const void *, DWORD, DWORD, void *, DWORD, DWORD, void *);
void GetFileSizeEx(HANDLE, LARGE_INTEGER *);
u32 WriteFile(HANDLE, const void *, DWORD, void *, OVERLAPPED *);
bool ReadFile(HANDLE, void *, DWORD, void *, OVERLAPPED *);
bool SetFilePointerEx(HANDLE, LARGE_INTEGER, void *, DWORD);
void CloseHandle(HANDLE);
#endif

namespace lr {
/// FILE SYSTEM ///
File::File(const fs::path &path, FileAccess access) {
    ZoneScoped;

    DWORD flags = 0;
    DWORD creation_flags = OPEN_EXISTING;
    DWORD share_flags = 0;
    if (access & FileAccess::Read) {
        flags |= GENERIC_READ;
        share_flags |= FILE_SHARE_READ;
    }
    if (access & FileAccess::Write) {
        flags |= GENERIC_WRITE;
        creation_flags |= CREATE_ALWAYS;
        share_flags |= FILE_SHARE_WRITE;
    }

    HANDLE file_handle = CreateFileW(path.c_str(), flags, share_flags, nullptr, creation_flags, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        this->result = FileResult::NoAccess;
        return;
    }

    LARGE_INTEGER li = {};
    GetFileSizeEx(file_handle, &li);

    this->size = li.QuadPart;
    this->handle = *reinterpret_cast<uptr *>(file_handle);
}

void File::write(this File &self, const void *data, ls::u64range range) {
    ZoneScoped;

    LR_CHECK(self.handle.has_value(), "Trying to write invalid file");

    HANDLE file_handle = reinterpret_cast<HANDLE>(self.handle.value());
    u64 offset = range.min;
    u64 written_bytes_size = 0;
    u64 target_size = range.length();
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + offset;
        DWORD cur_written_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = offset & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32;
        if (WriteFile(file_handle, cur_data, remainder_size, &cur_written_size, &overlapped) == 0) {
            LR_LOG_TRACE("File write interrupted! {}", cur_written_size);
            break;
        }

        offset += cur_written_size;
        written_bytes_size += cur_written_size;
    }
}

u64 File::read(this File &self, void *data, ls::u64range range) {
    ZoneScoped;

    LR_CHECK(self.handle.has_value(), "Trying to read invalid file");

    auto file_handle = reinterpret_cast<HANDLE>(self.handle.value());
    range.clamp(self.size);
    u64 offset = range.min;
    u64 read_bytes_size = 0;
    u64 target_size = range.length();
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;
        DWORD cur_read_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = offset & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32u;
        ReadFile(file_handle, cur_data, remainder_size, &cur_read_size, &overlapped);
        if (cur_read_size < 0) {
            LR_LOG_TRACE("File read interrupted! {}", cur_read_size);
            break;
        }

        offset += cur_read_size;
        read_bytes_size += cur_read_size;
    }

    return read_bytes_size;
}

void File::seek(this File &self, u64 offset) {
    ZoneScoped;

    LARGE_INTEGER li = {};
    li.QuadPart = offset;
    SetFilePointerEx(reinterpret_cast<HANDLE>(self.handle.value()), li, nullptr, FILE_BEGIN);
}

void File::close(this File &self) {
    ZoneScoped;

    HANDLE file_handle = reinterpret_cast<HANDLE>(self.handle.value());
    CloseHandle(file_handle);
    self.handle.reset();
}

/// MEMORY ///
u64 mem_page_size() {
    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

void *mem_reserve(u64 size) {
    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

void mem_release(void *data, u64 size) {
    TracyFree(data);
    VirtualFree(data, size, MEM_RELEASE);
}

bool mem_commit(void *data, u64 size) {
    TracyAllocN(data, size, "Virtual Alloc");
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void mem_decommit(void *data, u64 size) {
    VirtualFree(data, size, MEM_DECOMMIT);
}
}  // namespace lr
