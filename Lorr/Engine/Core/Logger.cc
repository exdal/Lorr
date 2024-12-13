#include "Engine/Core/Logger.hh"

#include "Engine/OS/File.hh"

namespace lr {
struct LoggerImpl {
    File file = {};

    void init(std::string_view log_name) {
        fs::path log_path = fs::current_path() / std::format("{}.log", log_name);
        this->file = File(log_path, FileAccess::Write);
    }

    ~LoggerImpl() { this->file.close(); }
};

static LoggerImpl GLOBAL_LOGGER;

void Logger::init(std::string_view name) {
    ZoneScoped;

    GLOBAL_LOGGER.init(name);
}

void Logger::to_file(std::string_view str) {
    ZoneScoped;

    // strip out console colors
    GLOBAL_LOGGER.file.write(str.data() + 5, str.length() - 5);
    File::to_stdout(str);
}

std::tm Logger::get_time() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    return *std::localtime(&time);
}
}  // namespace lr
