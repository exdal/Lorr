#pragma once

namespace lr::Memory
{
struct MemoryView
{
    MemoryView() = default;
    MemoryView(u8 *pData, usize size);
    u8 *Get(usize size);

    template<typename _T>
    _T &Get();

    u8 *m_pData = nullptr;
    usize m_Size = 0;
};

template<typename _T>
_T &MemoryView::Get()
{
    return *(_T &*)Get(sizeof(_T));
}

}  // namespace lr::Memory