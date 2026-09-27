
#include "logging.hpp"
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace
{
constexpr const char *kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%-8l] %v";
constexpr const char *kLogFile = "vcat.log";
constexpr size_t kMaxFileSize = 5 * 1024 * 1024;
constexpr size_t kMaxFiles = 3;

// The TUI owns the terminal, so console output is off by default.
constexpr bool kConsoleOutput = false;

bool g_initialized = false;

spdlog::level::level_enum toSpdLevel(logging::Level level)
{
    switch (level)
    {
    case logging::Level::TRACE:
        return spdlog::level::trace;
    case logging::Level::DEBUG:
        return spdlog::level::debug;
    case logging::Level::INFO:
        return spdlog::level::info;
    case logging::Level::WARN:
        return spdlog::level::warn;
    case logging::Level::ERROR:
        return spdlog::level::err;
    case logging::Level::CRITICAL:
        return spdlog::level::critical;
    }
    return spdlog::level::info;
}
} // namespace

namespace logging
{
void init()
{
    if (g_initialized)
    {
        return;
    }
    g_initialized = true;

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(kLogFile, kMaxFileSize, kMaxFiles));
    if (kConsoleOutput)
    {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>("vcat", sinks.begin(), sinks.end());
    logger->set_pattern(kPattern);
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(logger));

    info("Logging initialized");
}

void write(Level level, std::string_view message)
{
    spdlog::logger *logger = spdlog::default_logger_raw();
    if (logger == nullptr)
    {
        return;
    }
    logger->log(toSpdLevel(level), "{}", message);
}

bool enabled(Level level)
{
    spdlog::logger *logger = spdlog::default_logger_raw();
    return logger != nullptr && logger->should_log(toSpdLevel(level));
}
} // namespace logging
