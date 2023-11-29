#pragma once

namespace lr::Memory
{
struct AllocationArea
{
    u64 consume_as_offset(u64 size);
    u8 *consume_as_data(u64 size);

    AllocationArea *m_next = nullptr;
    u64 m_capacity = 0;
    u64 m_size = 0;
    u8 *m_data = nullptr;  // Also user-data
};

struct AreaAllocatorView
{
    bool can_allocate(u64 size, u32 alignment);
    void reset();
    u64 size();

    AllocationArea *find_free_area(u64 size);
    AllocationArea *m_first_area = nullptr;
};

struct AreaAllocator : AreaAllocatorView
{
    void init(u64 data_size);
    ~AreaAllocator();

    AllocationArea *allocate_area(usize size);
    AllocationArea *first_area() { return m_view.m_first_area; }

    bool can_allocate(u64 size, u32 alignment = 1);
    u8 *allocate(u64 size, u32 alignment = 1);
    template<typename _T, typename... _Args>
    _T *allocate_obj(_Args &&...args);

    AreaAllocatorView m_view = {};
    u64 m_region_size = 0;
};

template<typename _T, typename... _Args>
_T *AreaAllocator::allocate_obj(_Args &&...args)
{
    return new (allocate(sizeof(_T), alignof(_T))) _T(eastl::forward<_Args>(args)...);
}

}  // namespace lr::Memory