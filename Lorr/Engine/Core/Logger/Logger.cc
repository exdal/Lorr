#include "Logger.hh"

namespace lr
{
    void Logger::Init()
    {
        eastl::vector<spdlog::sink_ptr> logSinks;

        logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("lorr.log", true));

        logSinks[0]->set_pattern("[%n] %Y-%m-%d_%T.%e | %5^%L%$ | %v");
        logSinks[1]->set_pattern("[%n] %Y-%m-%d_%T.%e | %L | %v");

        s_pCoreLogger = std::make_shared<spdlog::logger>("LR", logSinks.begin(), logSinks.end());
        spdlog::register_logger(s_pCoreLogger);

        s_pCoreLogger->set_level(spdlog::level::trace);
        s_pCoreLogger->flush_on(spdlog::level::err);
    }

}  // namespace lr