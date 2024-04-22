#pragma once

namespace lr::os {
u64 page_size();
void *reserve(u64 size);
void release(void *data, u64 size = 0);
bool commit(void *data, u64 size);
void decommit(void *data, u64 size);
}  // namespace lr::os
