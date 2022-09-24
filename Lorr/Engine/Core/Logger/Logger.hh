//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if _DEBUG
#define LOG_TRACE(...) lr::Logger::s_pCoreLogger->trace(__VA_ARGS__)
#else
#define LOG_TRACE(...) (void *)0
#endif

#define LOG_INFO(...) lr::Logger::s_pCoreLogger->info(__VA_ARGS__)
#define LOG_WARN(...) lr::Logger::s_pCoreLogger->warn(__VA_ARGS__)
#define LOG_ERROR(...) lr::Logger::s_pCoreLogger->error(__VA_ARGS__);

#define LOG_CRITICAL(...)                                                                                                                            \
    {                                                                                                                                                \
        lr::Logger::s_pCoreLogger->critical(__VA_ARGS__);                                                                                            \
        lr::Logger::s_pCoreLogger->dump_backtrace();                                                                                                 \
        lr::Logger::s_pCoreLogger->flush();                                                                                                          \
        abort();                                                                                                                                     \
    }

namespace lr
{
    class Logger
    {
    public:
        Logger() = default;

        static void Init();

        inline static std::shared_ptr<spdlog::logger> s_pCoreLogger = nullptr;
    };

}  // namespace lr