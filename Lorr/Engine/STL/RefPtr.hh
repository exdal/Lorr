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

    ref_ptr(_T *pObj)
        : m_pObj(pObj)
    {
        IncRef();
    }

    ref_ptr(const this_type &other)
        : m_pObj(other.m_pObj),
          m_Ref(other.m_Ref)
    {
        IncRef();
    }

    ref_ptr(this_type &&other)
        : m_pObj(nullptr)
    {
        swap(other);
    }

    template<typename _U>
    ref_ptr(const ref_ptr<_U> &other)
        : m_pObj(other.m_pObj)
    {
        IncRef();
    }

    ~ref_ptr()
    {
        if (DecRef() == 0)
        {
            _Deleter del;
            del(m_pObj);
            m_pObj = nullptr;
        }
    }

    void swap(this_type &other)
    {
        _T *pTemp = m_pObj;
        m_pObj = other.m_pObj;
        other.m_pObject = pTemp;
    }

    this_type &operator=(this_type &other)
    {
        if (other.m_pObj != m_pObj)
        {
            _T *pTemp = m_pObj;
            if (other.m_pObj)
                other.IncRef();
            m_pObj = other.m_pObj;
            if (pTemp)
                DecRef();
        }

        return *this;
    }

    this_type &operator=(this_type &&other)
    {
        swap(other);
        return *this;
    }

    template<typename _U>
    this_type &operator=(const ref_ptr<_U> &other)
    {
        return operator=(other);
    }

    _T *get() const { return m_pObj; }
    _T &operator*() const { return *m_pObj; }
    _T *operator->() const { return m_pObj; }
    bool operator!() const { return (m_pObj == nullptr); }

    u64 m_Ref = 0;
    _T *m_pObj = nullptr;
};

template<typename _T, typename _Deleter = array_deleter<_T>>
struct ref_array
{
    typedef ref_array<_T, _Deleter> this_type;

    u64 IncRef() { return ++m_Ref; }
    u64 DecRef() { return --m_Ref; }

    ref_array(_T *pObj)
        : m_pObj(pObj)
    {
        IncRef();
    }

    ref_array(const this_type &other)
        : m_pObj(other.m_pObj),
          m_Ref(other.m_Ref)
    {
        IncRef();
    }

    ref_array(this_type &&other)
        : m_pObj(nullptr)
    {
        swap(other);
    }

    template<typename _U>
    ref_array(const ref_ptr<_U> &other)
        : m_pObj(other.m_pObj)
    {
        IncRef();
    }

    ~ref_array()
    {
        if (DecRef() == 0)
        {
            _Deleter del;
            del(m_pObj);
            m_pObj = nullptr;
        }
    }

    void swap(this_type &other)
    {
        _T *pTemp = m_pObj;
        m_pObj = other.m_pObj;
        other.m_pObj = pTemp;
    }

    this_type &operator=(this_type &other)
    {
        if (other.m_pObj != m_pObj)
        {
            _T *pTemp = m_pObj;
            if (other.m_pObj)
                other.IncRef();
            m_pObj = other.m_pObj;
            if (pTemp)
                DecRef();
        }

        return *this;
    }

    this_type &operator=(this_type &&other)
    {
        swap(other);
        return *this;
    }

    template<typename _U>
    this_type &operator=(const ref_array<_U> &other)
    {
        return operator=(other);
    }

    _T *get() const { return m_pObj; }
    bool operator!() const { return (m_pObj == nullptr); }
    _T &operator[](usize i) const { return m_pObj[i]; }

    u64 m_Ref = 0;
    _T *m_pObj = nullptr;
};

}  // namespace ls
