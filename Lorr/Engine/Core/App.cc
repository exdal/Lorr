#include "Engine/Core/App.hh"

#include "Engine/OS/Timer.hh"

namespace lr {
static ls::option<App> APP = ls::nullopt;

auto log_cb(
    [[maybe_unused]] i64 ns,
    [[maybe_unused]] fmtlog::LogLevel level,
    [[maybe_unused]] fmt::string_view location,
    [[maybe_unused]] usize basePos,
    [[maybe_unused]] fmt::string_view threadName,
    [[maybe_unused]] fmt::string_view msg,
    [[maybe_unused]] usize bodyPos,
    [[maybe_unused]] usize logFilePos
) -> void {
    ZoneScoped;

    fmt::println("{}", msg);
}

auto App::get() -> App & {
    return APP.value();
}

App::App(u32 worker_count_, ModuleRegistry &&modules_) : should_close(false), job_man(worker_count_), modules(std::move(modules_)) {
    ZoneScoped;
}

void App::run(this App &self) {
    ZoneScoped;

    self.job_man.wait();
    self.modules.init();
    self.job_man.wait();

    Timer timer;
    while (!self.should_close) {
        auto delta_time = timer.elapsed();
        timer.reset();

        self.modules.update(delta_time);

        fmtlog::poll();

        FrameMark;
    }

    self.shutdown();
}

void App::shutdown(this App &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");

    self.job_man.wait();
    self.should_close = true;
    self.modules.destroy();

    LOG_INFO("Complete!");

    fmtlog::poll(true);
}

AppBuilder::AppBuilder() {
    fmtlog::setThreadName("Main");
    fmtlog::setLogCB(log_cb, fmtlog::DBG);
    fmtlog::setHeaderPattern("[{HMSF}] [{t:<9}] {l}: ");
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::flushOn(fmtlog::WRN);
}

auto AppBuilder::build(this AppBuilder &self, u32 worker_count, const c8 *log_file_name) -> bool {
    LS_EXPECT(!APP.has_value());

    fmtlog::setLogFile(log_file_name, true);

    APP.emplace(worker_count, std::move(self.registry));
    APP->run();

    return true;
}

} // namespace lr
