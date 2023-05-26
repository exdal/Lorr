// Created on Thursday May 5th 2022 by exdal
// Last modified on Wednesday May 24th 2023 by exdal

#pragma once

#define LOG_TRACE(...) lr::Logger::s_pCoreLogger->trace(__VA_ARGS__)
#define LOG_INFO(...) lr::Logger::s_pCoreLogger->info(__VA_ARGS__)
#define LOG_WARN(...) lr::Logger::s_pCoreLogger->warn(__VA_ARGS__)
#define LOG_ERROR(...) lr::Logger::s_pCoreLogger->error(__VA_ARGS__)
#define LOG_CRITICAL(...) lr::Logger::s_pCoreLogger->critical(__VA_ARGS__)

#include <spdlog/spdlog.h>

namespace lr
{
struct Logger
{
    static void Init();
    inline static std::shared_ptr<spdlog::logger> s_pCoreLogger = nullptr;
};
}  // namespace lr