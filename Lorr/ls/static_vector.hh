#pragma once

#include <algorithm>
#include <cassert>

namespace ls {
template<typename T, size_t Capacity>
struct static_vector {
    static_assert(Capacity != 0, "static vectors cannot have zero capacity");

    typedef static_vector<T, Capacity> this_type;
    typedef T element_type;
    typedef std::remove_cv_t<T> value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef T *iterator;
    typedef const T *const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    constexpr static_vector() noexcept = default;
    constexpr static_vector(size_type count) noexcept
        requires(std::is_default_constructible_v<T>)
        : m_size(count) {
        std::uninitialized_default_construct_n(begin(), count);
    }

    explicit constexpr static_vector(const_iterator begin_it, const_iterator end_it) {
        m_size = std::distance(begin_it, end_it);
        assert(m_size <= capacity());
        std::uninitialized_copy(begin_it, end_it, begin());
    }

    explicit constexpr static_vector(iterator begin_it, iterator end_it) {
        m_size = std::distance(begin_it, end_it);
        assert(m_size <= capacity());
        std::uninitialized_copy(begin_it, end_it, begin());
    }

    constexpr static_vector(const this_type &other) noexcept
        : static_vector(other.begin(), other.end()) {}

    constexpr static_vector(pointer ptr, size_type size)
        : static_vector(ptr, ptr + size) {}

    constexpr static_vector(std::initializer_list<value_type> v)
        : static_vector(v.begin(), v.end()) {}

    template<size_t N>
    constexpr static_vector(element_type (&arr)[N]) noexcept
        : static_vector(arr, N) {}

    template<size_t N>
    constexpr static_vector(std::array<value_type, N> &arr) noexcept
        : static_vector(arr.begin(), arr.end()) {}

    template<size_t N>
    constexpr static_vector(const std::array<value_type, N> &arr) noexcept
        : static_vector(arr.begin(), arr.end()) {}

    constexpr static_vector(this_type &&other) noexcept
        requires(std::movable<T>)
        : m_size(std::move(other.size())) {
        std::uninitialized_move(other.begin(), other.end(), begin());
    }

    ~static_vector() noexcept { clear(); }

    constexpr pointer data() noexcept { return m_data; }
    constexpr const_pointer data() const noexcept { return m_data; }
    constexpr size_type capacity() const noexcept { return Capacity; }
    constexpr size_type max_size() const noexcept { return Capacity; }
    constexpr size_type size() const noexcept { return m_size; }
    constexpr size_type size_bytes() const noexcept { return m_size * sizeof(T); }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr bool full() const noexcept { return m_size == Capacity; }
    constexpr void clear() noexcept {
        if (!std::is_trivially_destructible_v<value_type>) {
            std::destroy(begin(), end());
        }

        m_size = 0;
    }

    // References
    constexpr reference front() { return m_data[0]; }
    constexpr const_reference front() const { return front(); }
    constexpr reference back() { return m_data[m_size - 1]; }
    constexpr const_reference back() const { return back(); }
    constexpr reference at(size_type i) {
        assert(i < size());
        return m_data[i];
    }
    constexpr const_reference at(size_type i) const { return at(i); }

    // Iterators
    constexpr iterator begin() noexcept { return data(); }
    constexpr const_iterator begin() const noexcept { return data(); }
    constexpr iterator end() noexcept { return data() + m_size; }
    constexpr const_iterator end() const noexcept { return data() + m_size; }
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr const_iterator cend() const noexcept { return end(); }
    constexpr reverse_iterator rbegin() const noexcept { return end(); }
    constexpr reverse_iterator rend() const noexcept { return begin(); }
    constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    // Modifiers
    constexpr iterator erase(const_iterator pos) {
        assert(pos >= begin() && pos <= end());
        iterator i = const_cast<iterator>(pos);
        std::destroy_at(i);
        std::move(i + 1, end(), i);
        m_size--;
        return i;
    }

    constexpr iterator erase(const_iterator first, const_iterator last) {
        if (first != last) {
            iterator pos = const_cast<value_type *>(std::move(const_cast<value_type *>(last), end(), const_cast<value_type *>(first)));
            std::destroy(pos, end());
            m_size -= (last - first);
        }

        return const_cast<value_type *>(first);
    }

    constexpr void resize(size_type count) {
        assert(count <= capacity());
        if (count < size()) {
            erase(begin() + count, end());
        } else {
            auto remaining = count - size();
            for (size_type i = 0; i < remaining; i++) {
                emplace_back();
            }
        }
    }

    constexpr void push_back(const T &v) {
        assert(!full());
        std::construct_at(end(), v);
        m_size++;
    }

    constexpr void push_back(T &&v) {
        assert(!full());
        std::construct_at(end(), std::move(v));
        m_size++;
    }

    template<typename... Args>
    constexpr reference emplace_back(Args &&...args) {
        assert(!full());
        auto pos = end();
        std::construct_at(pos, std::forward<Args>(args)...);
        m_size++;
        return *pos;
    }

    constexpr void pop_back() {
        m_size--;
        std::destroy_at(end());
    }

    // Operators
    constexpr reference operator[](size_type i) {
        assert(i <= m_size);
        return m_data[i];
    }

    constexpr const_reference operator[](size_type i) const {
        assert(i <= m_size);
        return m_data[i];
    }

    constexpr reference operator()(size_type i) const {
        assert(i <= m_size);
        return m_data[i];
    }

    constexpr this_type &operator=(const this_type &other) noexcept {
        if (&other == this) {
            return *this;
        }

        clear();
        m_size = other.size();
        std::uninitialized_copy(other.begin(), other.end(), begin());
        return *this;
    }

    constexpr this_type &operator=(this_type &&other) noexcept {
        if (&other == this) {
            return *this;
        }

        clear();
        m_size = std::move(other.m_size);
        std::uninitialized_move(other.begin(), other.end(), begin());
        return *this;
    }

    constexpr this_type &operator=(std::initializer_list<value_type> initl) noexcept {
        assert(initl.size() <= capacity());

        clear();
        m_size = initl.size();
        std::uninitialized_copy(initl.begin(), initl.end(), begin());
        return *this;
    }

private:
    size_type m_size = 0;

    union {
        value_type m_data[Capacity] = {};
        alignas(T[Capacity]) std::byte m_bytes[sizeof(T) * Capacity];
    };
};

/// OPERATORS ///
template<typename T, usize NT, typename U, usize NU>
constexpr bool operator==(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return (lhs.size() == rhs.size()) && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator<(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator!=(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return !(lhs == rhs);
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator<=(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return !(rhs < lhs);
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator>(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return rhs < lhs;
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator>=(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return !(lhs < rhs);
}

template<typename T, usize NT, typename U, usize NU>
constexpr bool operator<=>(static_vector<T, NT> lhs, static_vector<U, NU> rhs) {
    return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

}  // namespace ls
