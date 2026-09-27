#include "logging.hpp"
#include "spdlog/common.h"
#include "spdlog/sinks/sink.h"

#include <cmath>
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace
{
constexpr const char *kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%-8l] [%-12n] %v";
constexpr const char *kLogFile = "vcat.log";
constexpr size_t kMaxFileSize = 5 * 1024 * 1024;
constexpr size_t kMaxFiles = 3;
} // namespace

namespace logging
{
void init()
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(kLogFile, kMaxFileSize, kMaxFiles);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(fileSink);
    // sinks.push_back(consoleSink);

    auto vcatLogger = std::make_shared<spdlog::logger>("vcat", sinks.begin(), sinks.end());
    vcatLogger->set_pattern(kPattern);
    vcatLogger->set_level(spdlog::level::debug);
    vcatLogger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(vcatLogger);

    auto gstLogger = std::make_shared<spdlog::logger>("gst", sinks.begin(), sinks.end());
    gstLogger->set_pattern(kPattern);
    gstLogger->set_level(spdlog::level::warn);
    gstLogger->flush_on(spdlog::level::warn);
    spdlog::register_logger(gstLogger);

    spdlog::info("Logging initialized");
}
} // namespace logging
