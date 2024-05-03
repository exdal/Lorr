#pragma once

namespace lr::memory {
struct ThreadStack {
    ThreadStack();
    ~ThreadStack();

    u8 *ptr = nullptr;
};

inline ThreadStack &GetThreadStack()
{
    thread_local ThreadStack stack;
    return stack;
}

struct ScopedStack {
    ScopedStack();
    ScopedStack(const ScopedStack &) = delete;
    ScopedStack(ScopedStack &&) = delete;
    ~ScopedStack();

    auto operator=(const ScopedStack &) = delete;
    auto operator=(ScopedStack &&) = delete;

    template<typename T>
    T *alloc()
    {
        auto &stack = GetThreadStack();
        T *v = reinterpret_cast<T *>(stack.ptr);
        stack.ptr = align_up(stack.ptr + sizeof(T), alignof(T));
        return v;
    }

    template<typename T>
    std::span<T> alloc(usize count)
    {
        auto &stack = GetThreadStack();
        T *v = reinterpret_cast<T *>(stack.ptr);
        stack.ptr = align_up(stack.ptr + sizeof(T) * count, alignof(T));
        return { v, count };
    }

    template<typename T, typename... ArgsT>
    std::span<T> alloc_n(ArgsT &&...args)
    {
        usize count = sizeof...(ArgsT);
        std::span<T> spn = alloc<T>(count);
        std::construct_at(reinterpret_cast<T *>(spn.data()), std::forward<ArgsT>(args)...);
        return spn;
    }

    template<typename... ArgsT>
    std::string_view format(const fmt::format_string<ArgsT...> fmt, ArgsT &&...args)
    {
        auto &stack = GetThreadStack();
        char *begin = reinterpret_cast<char *>(stack.ptr);
        char *end = fmt::vformat_to(begin, fmt.get(), fmt::make_format_args(args...));
        *end = '\0';
        stack.ptr = align_up(reinterpret_cast<u8 *>(end + 1), 8);
        return { begin, end };
    }

    u8 *ptr = nullptr;
};
}  // namespace lr::memory
