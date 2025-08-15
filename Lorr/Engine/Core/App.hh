#pragma once

#include "Engine/Core/JobManager.hh"
#include "Engine/Core/Module.hh"

namespace lr {
struct AppBuilder;
struct App {
    bool should_close;
    JobManager job_man;
    ModuleRegistry modules;

public:
    static auto get() -> App &;

    template<typename T>
    static auto mod() -> T & {
        return get().modules.get<T>();
    }

    static auto close() -> void {
        get().should_close = true;
    }

    static auto submit_job(Arc<Job> job, bool prioritize = false) -> void {
        get().job_man.submit(std::move(job), prioritize);
    }

public:
    App(u32 worker_count_, ModuleRegistry &&modules_);
    void run(this App &);
    void shutdown(this App &);

    friend AppBuilder;
};

struct AppBuilder {
private:
    ModuleRegistry registry = {};

public:
    AppBuilder();

    template<typename T, typename... Args>
    auto module(Args &&...args) -> AppBuilder & {
        ZoneScoped;

        registry.add<T>(std::forward<Args>(args)...);

        return *this;
    }

    auto build(this AppBuilder &, u32 worker_count, const c8 *log_file_name) -> bool;
};

} // namespace lr
