#pragma once

#include <fmtlog/fmtlog.h>

#define LOG_TRACE(...) FMTLOG(fmtlog::DBG, __VA_ARGS__)
#define LOG_INFO(...) FMTLOG(fmtlog::INF, __VA_ARGS__)
#define LOG_WARN(...) \
    FMTLOG(fmtlog::WRN, __VA_ARGS__); \
    fmtlog::poll()
#define LOG_ERROR(...) \
    FMTLOG(fmtlog::ERR, __VA_ARGS__); \
    fmtlog::poll()
#define LOG_FATAL(...) \
    FMTLOG(fmtlog::ERR, __VA_ARGS__); \
    fmtlog::poll()
