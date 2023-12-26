#pragma once

#include "Deleter.hh"
#include "Types.hh"

namespace ls
{
template<typename T, typename TDeleter = ptr_deleter<T>>
struct ref_ptr
{
    typedef ref_ptr this_type;

    u64 inc_ref() { return ++m_ref; }
    u64 dec_ref() { return --m_ref; }

    explicit ref_ptr(T *obj)
        : m_obj(obj)
    {
        inc_ref();
    }

    ref_ptr(const this_type &other)
        : m_obj(other.m_obj),
          m_ref(other.m_ref)
    {
        inc_ref();
    }

    ref_ptr(this_type &&other) noexcept
        : m_obj(nullptr)
    {
        swap(other);
    }

    template<typename TU>
    explicit ref_ptr(const ref_ptr<TU> &other)
        : m_obj(other.m_obj)
    {
        inc_ref();
    }

    ~ref_ptr()
    {
        if (dec_ref() == 0)
        {
            TDeleter del;
            del(m_obj);
            m_obj = nullptr;
        }
    }

    void swap(this_type &other) noexcept
    {
        T *pTemp = m_obj;
        m_obj = other.m_obj;
        other.m_obj = pTemp;
    }

    this_type &operator=(this_type &other)
    {
        if (other.m_obj != m_obj)
        {
            const T *temp = m_obj;
            if (other.m_obj)
                other.inc_ref();
            m_obj = other.m_obj;
            if (temp)
                dec_ref();
        }

        return *this;
    }

    this_type &operator=(this_type &&other) noexcept
    {
        swap(other);
        return *this;
    }

    template<typename _U>
    this_type &operator=(const ref_ptr<_U> &other)
    {
        return operator=(other);
    }

    T *get() const { return m_obj; }
    T &operator*() const { return *m_obj; }
    T *operator->() const { return m_obj; }
    bool operator!() const { return (m_obj == nullptr); }

    u64 m_ref = 0;
    T *m_obj = nullptr;
};

template<typename T, typename TDeleter = array_deleter<T>>
struct ref_array
{
    typedef ref_array this_type;

    u64 inc_ref() { return ++m_ref; }
    u64 dec_ref() { return --m_ref; }

    explicit ref_array(T *obj)
        : m_obj(obj)
    {
        inc_ref();
    }

    ref_array(const this_type &other)
        : m_obj(other.m_obj),
          m_ref(other.m_ref)
    {
        inc_ref();
    }

    ref_array(this_type &&other) noexcept
        : m_obj(nullptr)
    {
        swap(other);
    }

    template<typename _U>
    explicit ref_array(const ref_ptr<_U> &other)
        : m_obj(other.m_obj)
    {
        inc_ref();
    }

    ~ref_array()
    {
        if (dec_ref() == 0)
        {
            TDeleter del;
            del(m_obj);
            m_obj = nullptr;
        }
    }

    void swap(this_type &other) noexcept
    {
        T *pTemp = m_obj;
        m_obj = other.m_obj;
        other.m_obj = pTemp;
    }

    this_type &operator=(this_type &other)
    {
        if (other.m_obj != m_obj)
        {
            const T *temp = m_obj;
            if (other.m_obj)
                other.inc_ref();
            m_obj = other.m_obj;
            if (temp)
                dec_ref();
        }

        return *this;
    }

    this_type &operator=(this_type &&other) noexcept
    {
        swap(other);
        return *this;
    }

    template<typename _U>
    this_type &operator=(const ref_array<_U> &other)
    {
        return operator=(other);
    }

    T *get() const { return m_obj; }
    bool operator!() const { return (m_obj == nullptr); }
    T &operator[](usize i) const { return m_obj[i]; }

    u64 m_ref = 0;
    T *m_obj = nullptr;
};

}  // namespace ls
