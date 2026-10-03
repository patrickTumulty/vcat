
#include "logging.hpp"
#include "spdlog/logger.h"
#include <cstdlib>
#include <filesystem>
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

// Number of log files kept when rotating: the current one plus four rolled-over copies.
constexpr size_t kMaxFiles = 5;

// The TUI owns the terminal, so console output is off by default.
constexpr bool kConsoleOutput = false;

bool g_initialized = false;

/**
 * @brief Resolves the path of the log file from the environment.
 *
 * Installed bundles export VCAT_LOG_DIR (to ~/.local/state/vcat) from the launcher, so their
 * logs land in $VCAT_LOG_DIR/log/vcat.log and survive reinstalls. Development builds leave the
 * variable unset and keep the historical behaviour of a vcat.log in the current working
 * directory. Directory creation is best effort: if it fails the rotating sink reports the
 * problem when it opens the file.
 *
 * @return <vcat_log_dir>/log/vcat.log when VCAT_LOG_DIR is set and non-empty, plain "vcat.log"
 *         otherwise.
 */
std::filesystem::path resolveLogFile()
{
    const char *logDir = std::getenv("VCAT_LOG_DIR");
    if (logDir == nullptr || *logDir == '\0')
    {
        return kLogFile;
    }

    std::filesystem::path dir = std::filesystem::path(logDir) / "log";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec); // best effort; the sink below reports real failures

    return dir / kLogFile;
}

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

std::atomic<std::shared_ptr<spdlog::logger>> loggerGlobal;

void logInit()
{
    if (g_initialized)
    {
        return;
    }
    g_initialized = true;

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(resolveLogFile().string(), kMaxFileSize, kMaxFiles));

    if (kConsoleOutput)
    {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>("vcat", sinks.begin(), sinks.end());
    loggerGlobal = logger;
    logger->set_pattern(kPattern);
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(logger));

    info("Logging initialized");
}

void logShutdown()
{
    spdlog::set_default_logger(nullptr);
    loggerGlobal.store(nullptr);
}

void logWrite(Level level, std::string_view message)
{
    auto logger = loggerGlobal.load();
    if (!logger)
    {
        return;
    }
    logger->log(toSpdLevel(level), "{}", message);
}

bool logSetLogLevel(Level level)
{
    spdlog::logger *logger = spdlog::default_logger_raw();
    return logger != nullptr && logger->should_log(toSpdLevel(level));
}
} // namespace logging
