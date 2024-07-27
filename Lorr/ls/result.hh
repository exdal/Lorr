#pragma once

namespace ls {
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
struct result {
    result() = default;
    result(ValT handle)
        : _val(static_cast<ValT &&>(handle)) {}

    result(ResultT result)
        : _result(result) {}

    result(ValT &&val, ResultT result)
        : _val(val),
          _result(result) {}

    explicit result(ValT val, ResultT result)
        : _val(val),
          _result(result) {}

    result &operator=(const result &) = default;
    result &operator=(result &&) = default;
    operator ValT &() { return _val; }
    operator ResultT() { return _result; }
    operator bool() { return _result; }

    ValT &val() { return _val; }
    ResultT &error() { return _result; }

    ValT _val = {};
    ResultT _result = {};
};
}  // namespace ls
