#pragma once

#define LOG_TRACE(...) FMTLOG(fmtlog::DBG, __VA_ARGS__)
#define LOG_INFO(...) FMTLOG(fmtlog::INF, __VA_ARGS__)
#define LOG_WARN(...) FMTLOG(fmtlog::WRN, __VA_ARGS__)
#define LOG_ERROR(...)                \
    FMTLOG(fmtlog::ERR, __VA_ARGS__); \
    assert(false);
#define LOG_CRITICAL(...)             \
    FMTLOG(fmtlog::ERR, __VA_ARGS__); \
    abort()

#ifdef _DEBUG
#define FMTLOG_NO_CHECK_LEVEL
#endif

#include "fmtlog.hh"

namespace lr
{
struct Logger
{
    static void Init();
};
}  // namespace lr
