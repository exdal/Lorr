#pragma once

namespace ls
{
template<typename _T>
struct ptr_deleter
{
    void operator()(const _T *p) const { delete const_cast<_T *>(p); }
};

template<>
struct ptr_deleter<void>
{
    void operator()(const void *p) const { delete[] (char *)p; };
};

template<>
struct ptr_deleter<const void>
{
    void operator()(const void *p) const { delete[] (char *)p; }
};

template<typename _T>
struct array_deleter
{
    void operator()(const _T *p) const { delete[] const_cast<_T *>(p); }
};

template<>
struct array_deleter<void>
{
    void operator()(const void *p) const { delete[] (char *)p; }
};

}  // namespace ls