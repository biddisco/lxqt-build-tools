// Lightweight spdlog-based logging infrastructure for grox
// Each source file creates a named logger via grox::log::create("name")
// which returns a shared_ptr<spdlog::logger>. This allows per-logger
// level control at runtime (e.g. via SPDLOG_LEVEL=logger_name=debug).
//
// Usage:
//   #include "debug/logging.hpp"
//   static auto logger = grox::log::create("MyModule");
//   // then use spdlog macros:
//   GROX_LOG_DEBUG(logger, "value is {}", 42);
//   GROX_LOG_ERROR(logger, "{:>20} something {}", "label", x);
//
#pragma once

#include <cstdlib>
#include <memory>
#include <string>
//
#include "grox/config-logging.hpp"

#if !defined(SPDLOG_ACTIVE_LEVEL)
# error "SPDLOG_ACTIVE_LEVEL is not defined. Rerun cmake."
#endif

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fmt/ostream.h>

// ---------------------------------------------------------------------------
// Logging macros that use a named logger.
//
// These wrap spdlog's SPDLOG_LOGGER_* macros so that compile-time filtering
// via SPDLOG_ACTIVE_LEVEL works correctly. The first argument is always a
// std::shared_ptr<spdlog::logger>.
// ---------------------------------------------------------------------------
#define GROX_LOG_TRACE(logger, ...) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__)
#define GROX_LOG_DEBUG(logger, ...) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__)
#define GROX_LOG_INFO(logger, ...) SPDLOG_LOGGER_INFO(logger, __VA_ARGS__)
#define GROX_LOG_WARN(logger, ...) SPDLOG_LOGGER_WARN(logger, __VA_ARGS__)
#define GROX_LOG_ERROR(logger, ...) SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__)
#define GROX_LOG_CRITICAL(logger, ...) SPDLOG_LOGGER_CRITICAL(logger, __VA_ARGS__)

namespace grox::log {

  // -------------------------------------------------------------------------
  /// Create (or retrieve) a named logger. Thread-safe. If the logger already
  /// exists in spdlog's registry it is returned; otherwise a new
  /// stderr-colour logger is created and registered.
  inline std::shared_ptr<spdlog::logger> create(std::string const& name)
  {
    auto logger = spdlog::get(name);
    if (!logger)
    {
      logger = spdlog::stderr_color_mt(name);
      // Inherit the global level that was set during initialisation
      logger->set_level(spdlog::get_level());
    }
    return logger;
  }

  // -------------------------------------------------------------------------
  /// Call once at application start-up (e.g. in main()) to configure the
  /// global log pattern and level from environment variables.
  ///
  /// Recognised variables:
  ///   GROX_LOG_PATTERN  /  SPDLOG_PATTERN   - log line pattern
  ///   GROX_LOG_LEVEL    /  SPDLOG_LEVEL     - global level + per-logger overrides
  ///
  /// If none are set a sensible default pattern is used.
  inline void init_from_env()
  {
    // --- pattern ---
    char const* pattern = std::getenv("GROX_LOG_PATTERN");
    if (!pattern || pattern[0] == '\0') pattern = std::getenv("SPDLOG_PATTERN");

    if (pattern && pattern[0] != '\0')
      spdlog::set_pattern(pattern);
    else
      spdlog::set_pattern("[%^%-8l%$] [%t] %n | %v");

    // set log level from thee user specified compilation level
    spdlog::set_level(static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL));
    // reads SPDLOG_LEVEL for per-logger overrides (e.g. "mylogger=debug,other=info") and applies them.
    spdlog::cfg::load_env_levels();

    // We additionally support GROX_LOG_LEVEL as an override.
    char const* level = std::getenv("GROX_LOG_LEVEL");
    if (level && level[0] != '\0')
    {
      if (static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL) >
          spdlog::level::from_str(level))
      {
        // If the user-specified level is more verbose than the compile-time level, log a warning.
        spdlog::log(spdlog::level::off,
            "GROX_LOG_LEVEL={} is more verbose than the compile-time level '{}', "
            "but will be applied anyway. (Recompile with a lower SPDLOG_ACTIVE_LEVEL).",
            level,
            spdlog::level::to_string_view(
                static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL)));
      }
      if (static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL) <
          spdlog::level::from_str(level))
      {
        // If the user-specified level is more verbose than the compile-time level, log a warning.
        spdlog::log(spdlog::level::off,
            "GROX_LOG_LEVEL={} is less verbose than the compile-time level '{}'", level,
            spdlog::level::to_string_view(
                static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL)));
      }
      spdlog::set_level(spdlog::level::from_str(level));
    }
  }

}    // namespace grox::log
