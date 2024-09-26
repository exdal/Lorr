#pragma once

#include <glaze/core/common.hpp>
#include <glaze/core/meta.hpp>
#include <glaze/json/read.hpp>

namespace lr {
struct generic_json {
    using array_t = std::vector<generic_json>;
    using object_t = ankerl::unordered_dense::map<std::string, generic_json>;
    using null_t = std::nullptr_t;
    using val_t = std::variant<null_t, std::string, bool, array_t, object_t, f64, glm::vec2, glm::vec3, glm::vec4>;
    val_t data{};

    template<class T>
    [[nodiscard]] T &get() {
        return std::get<T>(data);
    }

    template<class T>
    [[nodiscard]] const T &get() const {
        return std::get<T>(data);
    }

    template<class T>
    [[nodiscard]] T *get_if() noexcept {
        return std::get_if<T>(&data);
    }

    template<class T>
    [[nodiscard]] const T *get_if() const noexcept {
        return std::get_if<T>(&data);
    }

    template<class T>
    [[nodiscard]] bool is() const noexcept {
        return std::holds_alternative<T>(data);
    }

    generic_json &operator[](std::integral auto &&index) { return std::get<array_t>(data)[index]; }

    const generic_json &operator[](std::integral auto &&index) const { return std::get<array_t>(data)[index]; }

    generic_json &operator[](std::convertible_to<std::string_view> auto &&key) {
        //[] operator for maps does not support heterogeneous lookups yet
        if (is<null_t>())
            data = object_t{};
        auto &object = std::get<object_t>(data);
        auto key_str = std::string(key);
        auto iter = object.find(key_str);
        if (iter == object.end()) {
            iter = object.insert(std::make_pair(key_str, generic_json{})).first;
        }
        return iter->second;
    }

    const generic_json &operator[](std::convertible_to<std::string_view> auto &&key) const {
        //[] operator for maps does not support heterogeneous lookups yet
        auto &object = std::get<object_t>(data);
        auto key_str = std::string(key);
        auto iter = object.find(key_str);
        if (iter == object.end()) {
            glz::glaze_error("Key not found.");
        }
        return iter->second;
    }

    [[nodiscard]] generic_json &at(std::convertible_to<std::string_view> auto &&key) { return operator[](key); }

    [[nodiscard]] const generic_json &at(std::convertible_to<std::string_view> auto &&key) const { return operator[](key); }

    [[nodiscard]] bool contains(std::convertible_to<std::string_view> auto &&key) const {
        if (!is<object_t>())
            return false;
        auto &object = std::get<object_t>(data);
        auto iter = object.find(key);
        return iter != object.end();
    }

    explicit operator bool() const { return !std::holds_alternative<null_t>(data); }

    val_t *operator->() noexcept { return &data; }

    val_t &operator*() noexcept { return data; }

    const val_t &operator*() const noexcept { return data; }

    void reset() noexcept { data = null_t{}; }

    generic_json() = default;
    generic_json(const generic_json &) = default;
    generic_json &operator=(const generic_json &) = default;
    generic_json(generic_json &&) = default;
    generic_json &operator=(generic_json &&) = default;

    template<class T>
        requires std::convertible_to<T, val_t> && (!std::same_as<generic_json, std::decay_t<T>>)
    generic_json(T &&val) {
        data = val;
    }

    template<class T>
        requires std::convertible_to<T, double> && (!std::same_as<generic_json, std::decay_t<T>>) && (!std::convertible_to<T, val_t>)
    generic_json(T &&val) {
        data = static_cast<double>(val);
    }

    generic_json(std::initializer_list<std::pair<const char *, generic_json>> &&obj) {
        // TODO try to see if there is a beter way to do this initialization withought copying the json_t
        // std::string in std::initializer_list<std::pair<const std::string, json_t>> would match with {"literal",
        // "other_literal"} So we cant use std::string or std::string view. Luckily const char * will not match with
        // {"literal", "other_literal"} but then we have to copy the data from the initializer list data =
        // object_t(obj); // This is what we would use if std::initializer_list<std::pair<const std::string, json_t>>
        // worked
        data.emplace<object_t>();
        auto &data_obj = std::get<object_t>(data);
        for (auto &&pair : obj) {
            data_obj.emplace(pair.first, pair.second);
        }
    }

    // Prevent conflict with object initializer list
    template<bool deprioritize = true>
    generic_json(std::initializer_list<generic_json> &&arr) {
        data.emplace<array_t>(arr);
    }

    template<class T>
    [[nodiscard]] T as() const {
        // Prefer get becuase it returns a reference
        return get<T>();
    }

    template<class T>
        requires std::convertible_to<double, T>
    [[nodiscard]] T as() const {
        // Can be used for int and the like
        return static_cast<T>(get<double>());
    }

    template<class T>
        requires std::convertible_to<std::string, T>
    [[nodiscard]] T as() const {
        // Can be used for string_view and the like
        return get<std::string>();
    }

    [[nodiscard]] bool is_array() const noexcept { return is<generic_json::array_t>(); }
    [[nodiscard]] bool is_object() const noexcept { return is<generic_json::object_t>(); }
    [[nodiscard]] bool is_string() const noexcept { return is<std::string>(); }
    [[nodiscard]] bool is_null() const noexcept { return is<std::nullptr_t>(); }
    [[nodiscard]] bool is_number() const noexcept { return is<f64>(); }

    [[nodiscard]] auto &get_array() noexcept { return get<array_t>(); }
    [[nodiscard]] const array_t &get_array() const noexcept { return get<array_t>(); }
    [[nodiscard]] auto &get_object() noexcept { return get<object_t>(); }
    [[nodiscard]] const auto &get_object() const noexcept { return get<object_t>(); }
    [[nodiscard]] auto &get_string() noexcept { return get<std::string>(); }
    [[nodiscard]] auto get_string() const noexcept { return get<std::string>(); }
    [[nodiscard]] auto &get_number() noexcept { return get<f64>(); }
    [[nodiscard]] const auto &get_number() const noexcept { return get<f64>(); }

    // empty() returns true if the value is an empty JSON object, array, or string, or a null value
    // otherwise returns false
    [[nodiscard]] bool empty() const noexcept {
        if (auto *v = get_if<object_t>()) {
            return v->empty();
        } else if (auto *v = get_if<array_t>()) {
            return v->empty();
        } else if (auto *v = get_if<std::string>()) {
            return v->empty();
        } else if (is_null()) {
            return true;
        } else {
            return false;
        }
    }

    // returns the count of items in an object or an array, or the size of a string, otherwise returns zero
    [[nodiscard]] size_t size() const noexcept {
        if (auto *v = get_if<object_t>()) {
            return v->size();
        } else if (auto *v = get_if<array_t>()) {
            return v->size();
        } else if (auto *v = get_if<std::string>()) {
            return v->size();
        } else {
            return 0;
        }
    }
};
}  // namespace lr

template<typename KeyT, typename ValueT>
struct glz::meta<ankerl::unordered_dense::map<KeyT, ValueT>> {
    static constexpr std::string_view name = glz::join_v<chars<"ankerl::unordered_dense::map<">, name_v<KeyT>, chars<",">, name_v<ValueT>, chars<">">>;
};

template<>
struct glz::meta<glm::vec2> {
    static constexpr std::string_view name = "glm::vec2";
    using T = glm::vec2;
    static constexpr auto value = object(&T::x, &T::y);
};

template<>
struct glz::meta<glm::vec3> {
    static constexpr std::string_view name = "glm::vec3";
    using T = glm::vec3;
    static constexpr auto value = object(&T::x, &T::y, &T::z);
};

template<>
struct glz::meta<glm::vec4> {
    static constexpr std::string_view name = "glm::vec4";
    using T = glm::vec4;
    static constexpr auto value = object(&T::x, &T::y, &T::z, &T::w);
};

template<>
struct glz::meta<lr::generic_json> {
    static constexpr std::string_view name = "glz::json_t";
    using T = lr::generic_json;
    static constexpr auto value = &T::data;
};
