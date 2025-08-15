#pragma once

#include <typeindex>

namespace lr {
template<typename T>
concept Module = requires(T t) {
    t.init();
    t.destroy();
    {
        T::MODULE_NAME
    } -> std::convertible_to<std::string_view>;
};

template<typename T>
concept ModuleHasUpdate = requires(T t, f64 delta_time) { t.update(delta_time); };

struct TypeIndexHash {
    std::size_t operator()(const std::type_index &ti) const noexcept {
        return ti.hash_code();
    }
};

struct ModuleRegistry {
    using ModulePtr = std::unique_ptr<void, void (*)(void *)>;
    using Registry = ankerl::unordered_dense::map<std::type_index, ModulePtr, TypeIndexHash>;

    Registry registry = {};
    std::vector<std::function<bool()>> init_callbacks = {};
    std::vector<ls::option<std::function<void(f64)>>> update_callbacks = {};
    std::vector<std::function<void()>> destroy_callbacks = {};
    std::vector<std::string_view> module_names = {};

    template<Module T, typename... Args>
    auto add(Args &&...args) -> void {
        ZoneScoped;

        auto type_index = std::type_index(typeid(T));
        auto deleter = [](void *self) { delete static_cast<T *>(self); };
        auto &module = registry.try_emplace(type_index, ModulePtr(new T(std::forward<Args>(args)...), deleter)).first->second;

        init_callbacks.push_back([module = static_cast<T *>(module.get())]() { return module->init(); });
        destroy_callbacks.push_back([module = static_cast<T *>(module.get())]() { module->destroy(); });
        if constexpr (ModuleHasUpdate<T>) {
            update_callbacks.push_back([module = static_cast<T *>(module.get())](f64 delta_time) { module->update(delta_time); });
        } else {
            update_callbacks.emplace_back(ls::nullopt);
        }
        module_names.push_back(T::MODULE_NAME);
    }

    template<typename T>
    auto has() -> bool {
        ZoneScoped;

        return registry.contains(std::type_index(typeid(T)));
    }

    template<typename T>
    auto get() -> T & {
        ZoneScoped;

        auto it = registry.find(std::type_index(typeid(T)));
        LS_EXPECT(it != registry.end());

        return *static_cast<T *>(it->second.get());
    }

    auto init(this ModuleRegistry &) -> bool;
    auto update(this ModuleRegistry &, f64 delta_time) -> void;
    auto destroy(this ModuleRegistry &) -> void;
};

} // namespace lr
