
#pragma once

#include <format>
#include <string_view>
#include <utility>

/**
 * Logging facade for the vcat libs.
 *
 * Every lib logs through here instead of talking to a logging backend directly, so the backend
 * (currently spdlog) is an implementation detail of vcat-common. Call sites keep the familiar
 * brace style formatting, e.g. logging::info("Resolution {}x{}", width, height).
 */
namespace logging
{

enum class Level
{
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

/// Sets up the sinks and the default logger. Safe to call more than once.
void init();

/// Logs an already formatted message.
void write(Level level, std::string_view message);

/// True if a message at `level` would be logged, so callers can skip expensive formatting.
bool enabled(Level level);

template <typename... Args> void log(Level level, std::format_string<Args...> message, Args &&...args)
{
    if (!enabled(level))
    {
        return;
    }
    write(level, std::format(message, std::forward<Args>(args)...));
}

template <typename... Args> void trace(std::format_string<Args...> message, Args &&...args)
{
    log(Level::TRACE, message, std::forward<Args>(args)...);
}

template <typename... Args> void debug(std::format_string<Args...> message, Args &&...args)
{
    log(Level::DEBUG, message, std::forward<Args>(args)...);
}

template <typename... Args> void info(std::format_string<Args...> message, Args &&...args)
{
    log(Level::INFO, message, std::forward<Args>(args)...);
}

template <typename... Args> void warn(std::format_string<Args...> message, Args &&...args)
{
    log(Level::WARN, message, std::forward<Args>(args)...);
}

template <typename... Args> void error(std::format_string<Args...> message, Args &&...args)
{
    log(Level::ERROR, message, std::forward<Args>(args)...);
}

template <typename... Args> void critical(std::format_string<Args...> message, Args &&...args)
{
    log(Level::CRITICAL, message, std::forward<Args>(args)...);
}

} // namespace logging
