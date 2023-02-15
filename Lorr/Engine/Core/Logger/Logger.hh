//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

#include "fmtlog.hh"

#define LOG_TRACE(...) FMTLOG(fmtlog::DBG, __VA_ARGS__)
#define LOG_INFO(...) FMTLOG(fmtlog::INF, __VA_ARGS__)
#define LOG_WARN(...) FMTLOG(fmtlog::WRN, __VA_ARGS__)
#define LOG_ERROR(...) FMTLOG(fmtlog::ERR, __VA_ARGS__)
#define LOG_CRITICAL(...) FMTLOG(fmtlog::ERR, __VA_ARGS__)

namespace lr::Logger
{
    void Init();
}  // namespace lr::Logger