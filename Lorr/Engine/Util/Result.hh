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

template<typename ValT, ResultConcept ResultT>
struct Result {
    Result() = default;
    Result(ValT handle)
        : m_val(std::move(handle))
    {
    }

    Result(ResultT result)
        : m_result(result)
    {
    }

    Result(ValT &&val, ResultT result)
        : m_val(val),
          m_result(result)
    {
    }

    explicit Result(ValT val, ResultT result)
        : m_val(val),
          m_result(result)
    {
    }

    Result &operator=(const Result &) = default;
    Result &operator=(Result &&) = default;
    operator ValT &() { return m_val; }
    operator ResultT() { return m_result; }
    operator bool() { return m_result; }

    ValT &val() { return m_val; }
    ResultT &result() { return m_result; }

    ValT m_val = {};
    ResultT m_result = {};
};
}  // namespace lr
