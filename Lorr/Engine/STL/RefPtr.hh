#pragma once

#include "Deleter.hh"
#include "Types.hh"

namespace ls
{
template<typename _T, typename _Deleter = ptr_deleter<_T>>
struct ref_ptr
{
    typedef ref_ptr<_T, _Deleter> this_type;

    u64 IncRef() { return ++m_Ref; }
    u64 DecRef() { return --m_Ref; }

    ref_ptr(_T *pObj);
    ref_ptr(const this_type &other);
    ~ref_ptr();

    this_type &operator=(this_type &other);

    _T *get() const { return m_pObj; }
    _T &operator*() const { return *m_pObj; }
    _T *operator->() const { return m_pObj; }
    bool operator!() const { return (m_pObj == nullptr); }

    u64 m_Ref = 0;
    _T *m_pObj = nullptr;
};

template<typename _T, typename _Deleter>
inline ref_ptr<_T, _Deleter>::ref_ptr(_T *pObj)
    : m_pObj(pObj)
{
    IncRef();
}

template<typename _T, typename _Deleter>
inline ref_ptr<_T, _Deleter>::ref_ptr(const this_type &other)
    : m_pObj(other.m_pObj),
      m_Ref(other.m_Ref)
{
    IncRef();
}

template<typename _T, typename _Deleter>
inline ref_ptr<_T, _Deleter>::~ref_ptr()
{
    if (DecRef() == 0)
    {
        _Deleter del;
        del(m_pObj);
        m_pObj = nullptr;
    }
}

template<typename _T, typename _Deleter>
inline ref_ptr<_T, _Deleter> &ref_ptr<_T, _Deleter>::operator=(this_type &other)
{
    if (other.m_pObj != m_pObj)
    {
        _T *const pTemp = m_pObj;
        if (other.m_pObj)
            other.IncRef();
        m_pObj = other.pObj;
        if (pTemp)
            DecRef();
    }

    return *this;
}

template<typename _T, typename _Deleter = array_deleter<_T>>
struct ref_array : ref_ptr<_T, _Deleter>
{
    _T &operator[](usize i) const { return this->m_pObj[i]; }
};

}  // namespace ls
