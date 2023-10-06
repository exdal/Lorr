#include "MemoryView.hh"

namespace lr::Memory
{
MemoryView::MemoryView(u8 *pData, usize size)
    : m_pData(pData),
      m_Size(size)
{
}

u8 *MemoryView::Get(usize size)
{
    assert(size <= m_Size);
    m_Size -= size;
    m_pData += size;
    return m_pData;
}
}  // namespace lr::Memory