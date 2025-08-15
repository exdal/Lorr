#include "Engine/Core/Module.hh"

namespace lr {
auto ModuleRegistry::init(this ModuleRegistry &self) -> bool {
    ZoneScoped;

    for (const auto &[name, cb] : std::views::zip(self.module_names, self.init_callbacks)) {
        auto start_ts = std::chrono::high_resolution_clock::now();
        if (!cb()) {
            LOG_ERROR("Module {} failed to initialize!", name);
            return false;
        }

        auto end_ts = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration<f64, std::milli>(end_ts - start_ts);
        LOG_INFO("Initialized module {} in {} ms.", name, delta.count());
    }

    return true;
}

auto ModuleRegistry::update(this ModuleRegistry &self, f64 delta_time) -> void {
    ZoneScoped;

    for (const auto &cb : self.update_callbacks) {
        if (LS_LIKELY(cb.has_value())) {
            cb.value()(delta_time);
        }
    }
}

auto ModuleRegistry::destroy(this ModuleRegistry &self) -> void {
    for (const auto &cb : std::views::reverse(self.destroy_callbacks)) {
        cb();
    }
}

} // namespace lr
