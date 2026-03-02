#pragma once

// Plugin API for dynamically loadable indicators
// This provides a C-compatible ABI for loading indicator plugins at runtime

#include <cstdint>
#include "debug/logging.hpp"

// ----------------------------------------------------------------------------
static auto plugin_api_log = grox::log::create("plug-api");

// #if __has_include(<spdlog/spdlog.h>)
// # include <spdlog/spdlog.h>
// # define GROX_PLUGIN_HAS_SPDLOG 1
// #else
// # define GROX_PLUGIN_HAS_SPDLOG 0
// #endif

// Forward declare the registry class to avoid C++ name mangling issues
namespace indicators {
  class indicator_registry;
}

// Plugin metadata structure
struct grox_plugin_info
{
  char const* name;
  char const* version;
  char const* description;
  char const* category;    // e.g., "moving_averages", "volatility", "trading"
  uint32_t api_version;    // For future compatibility checking
};

// Current API version (major.minor encoded as uint32)
#define GROX_PLUGIN_API_VERSION ((1u << 16) | 0u)    // Version 1.0

// Plugin entry point signature
// Each plugin must export a function with this signature
extern "C" {
typedef void (*grox_plugin_register_func)(indicators::indicator_registry* registry);
typedef grox_plugin_info const* (*grox_plugin_info_func)();
}

// Convenience macros for plugin implementation
#define GROX_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))

// Macro to define plugin info
#define GROX_DEFINE_PLUGIN_INFO(plugin_name, plugin_version, plugin_desc, plugin_category)         \
  GROX_PLUGIN_EXPORT const grox_plugin_info* grox_plugin_get_info()                                \
  {                                                                                                \
    GROX_LOG_TRACE(plugin_api_log, "{:>20} name={} version={} category={}", "plugin_get_info",     \
        plugin_name, plugin_version, plugin_category);                                             \
    static const grox_plugin_info info = {                                                         \
        plugin_name, plugin_version, plugin_desc, plugin_category, GROX_PLUGIN_API_VERSION};       \
    return &info;                                                                                  \
  }

// Macro to begin plugin registration
#define GROX_BEGIN_PLUGIN_REGISTRATION()                                                           \
  GROX_PLUGIN_EXPORT void grox_plugin_register(indicators::indicator_registry* registry)           \
  {                                                                                                \
    GROX_LOG_TRACE(plugin_api_log, "{:>20} called registry_ptr={}", "plugin_register",             \
        static_cast<void*>(registry));                                                             \
    using namespace indicators;

// Macro to end plugin registration
#define GROX_END_PLUGIN_REGISTRATION() }
