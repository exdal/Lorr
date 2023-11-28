#include "Logger.hh"
#include "fmtlog.hh"

namespace lr
{
void logcb(
    int64_t ns,
    fmtlog::LogLevel level,
    fmt::string_view location,
    size_t basePos,
    fmt::string_view threadName,
    fmt::string_view msg,
    size_t bodyPos,
    size_t logFilePos)
{
    fwrite(msg.data(), msg.size(), 1, stdout);
    fflush(stdout);
}

void Logger::Init()
{
    fmtlog::setLogCB(logcb, fmtlog::DBG);
    fmtlog::setLogFile("engine.log", true);
    fmtlog::setHeaderPattern("{HMSf} | {t:<6} | {s:<16} | {l} | ");
    fmtlog::flushOn(fmtlog::DBG);
    fmtlog::setThreadName("MAIN");
    fmtlog::startPollingThread(1);
}

}  // namespace lr