#include "Logger.hh"

#include "fmtlog-inl.hh"

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
        msg.remove_prefix(bodyPos);
        fmt::print("{}\n", msg);
    }

    void Logger::Init()
    {
        fmtlog::setLogCB(logcb, fmtlog::DBG);
        fmtlog::setLogFile("lorr.log", true);
        fmtlog::setHeaderPattern("{HMSf} | {t:<6} | {l} | ");
        fmtlog::flushOn(fmtlog::DBG);
        fmtlog::setThreadName("MAIN");
        fmtlog::startPollingThread(10000);
    }

}  // namespace lr