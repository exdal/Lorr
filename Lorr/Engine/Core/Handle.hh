#pragma once

namespace lr {
template<typename T>
struct Handle {
    struct Impl;

protected:
    Impl *impl = nullptr;

public:
    Handle() = default;
    explicit Handle(std::nullptr_t)
        : impl(nullptr) {};
    Handle(Impl *impl_)
        : impl(impl_) {};

    operator T() const noexcept { return T(impl); }
    T unwrap() const noexcept { return T(impl); }

    operator bool() const noexcept { return impl; }
    auto operator->() const noexcept { return impl; }

    bool operator==(Handle other) const noexcept { return impl == other.impl; }
};

}  // namespace lr