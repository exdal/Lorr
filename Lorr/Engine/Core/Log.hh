#pragma once

#include <loguru.hpp>

namespace lr {
struct Log {
    static inline void init(i32 argc, char **argv)
    {
        loguru::init(argc, argv);
        loguru::add_file("engine.log", loguru::Truncate, loguru::Verbosity_MAX);
    }
};

#define LR_LOG_TRACE(...) DLOG_F(INFO, __VA_ARGS__)
#define LR_LOG_INFO(...) LOG_F(INFO, __VA_ARGS__)
#define LR_LOG_WARN(...) LOG_F(WARN, __VA_ARGS__)
#define LR_LOG_ERROR(...) LOG_F(ERROR, __VA_ARGS__)
#define LR_LOG_FATAL(...) LOG_F(FATAL, __VA_ARGS__)

#define LR_ASSERT(test, ...) CHECK_F(test, ##__VA_ARGS__)
}  // namespace lr
