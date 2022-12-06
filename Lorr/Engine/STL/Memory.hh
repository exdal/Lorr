//
// Created on Saturday 26th November 2022 by e-erdal
//

#pragma once

namespace ls
{
    template<typename _T>
    class scoped_ptr_impl
    {
    public:
        _T &operator*() const
        {
            return m_pPtr;
        }

        _T *operator->() const
        {
            return m_pPtr;
        }

        _T *get()
        {
            return m_pPtr;
        }

        _T **get_address()
        {
            return &m_pPtr;
        }

    protected:
        _T *m_pPtr;
    };

    template<typename _T>
    class scoped_ptr : public scoped_ptr_impl<_T>
    {
        using scoped_ptr_impl<_T>::m_pPtr;

    public:
        explicit scoped_ptr(_T *pPtr = nullptr)
        {
            m_pPtr = pPtr;
        }

        ~scoped_ptr()
        {
            if (m_pPtr)
                delete m_pPtr;

            m_pPtr = nullptr;
        }
    };

    // Scoped pointers for `IUnknown` aka the windows/dx pointers
    template<typename _T>
    class scoped_comptr : public scoped_ptr_impl<_T>
    {
        using scoped_ptr_impl<_T>::m_pPtr;
        static_assert(eastl::is_base_of<IUnknown, _T>::value, "_T must be base of IUnknown.");

    public:
        explicit scoped_comptr(_T *pPtr = nullptr)
        {
            this->m_pPtr = pPtr;
        }

        ~scoped_comptr()
        {
            if (m_pPtr)
                m_pPtr->Release();

            m_pPtr = nullptr;
        }
    };

#define LS_IID_PTR(cptr) __uuidof(*(cptr.get())), (void **)cptr.get_address()

}  // namespace ls