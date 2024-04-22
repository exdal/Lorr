#pragma once

namespace lr {
// Thanks to RVO, it's okay to return by value.
// The performance impact is almost non-existent.
// There might be cases like NRVO, where it's the
// compiler decides what to do, anyway, just dont over use it
// https://en.cppreference.com/w/cpp/language/copy_elision
//
// But still, use light or trivial objects with this

template<typename ResultT>
concept ResultConcept = requires(ResultT v) {
    // operator!(v);
    !v;
};

template<typename HandleT, ResultConcept ResultT>
struct Result {
    Result() = default;
    Result(HandleT handle)
        : m_handle(std::move(handle))
    {
    }

    Result(ResultT result)
        : m_result(result)
    {
    }

    Result(HandleT &&handle, ResultT result)
        : m_handle(handle),
          m_result(result)
    {
    }

    explicit Result(HandleT handle, ResultT result)
        : m_handle(handle),
          m_result(result)
    {
    }

    Result &operator=(const Result &) = default;
    Result &operator=(Result &&) = default;
    operator HandleT &() { return m_handle; }
    operator ResultT() { return m_result; }
    operator bool() { return m_result; }

    HandleT &handle() { return m_handle; }
    ResultT &result() { return m_result; }

    HandleT m_handle = {};
    ResultT m_result = {};
};

}  // namespace lr
