#include "indicators/plugin_loader.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <iostream>
//
#include "debug/logging.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"

namespace fs = std::filesystem;

namespace indicators {

  // ----------------------------------------------------------------------------
  plugin_loader::~plugin_loader() { unload_all(); }

  // ----------------------------------------------------------------------------
  std::size_t plugin_loader::load_plugins_from_directory(
      std::string const& directory, indicator_registry& registry)
  {
    GROX_LOG_DEBUG(indicator_log, "{:>20} {}", "Loading plugins from", directory);

    if (!fs::exists(directory) || !fs::is_directory(directory))
    {
      GROX_LOG_WARN(indicator_log, "{:>20} does not exist: {}", "Plugin directory", directory);
      return 0;
    }

    std::size_t loaded_count = 0;

    try
    {
      for (auto const& entry : fs::directory_iterator(directory))
      {
        if (!entry.is_regular_file()) continue;

        std::string const path = entry.path().string();
        std::string const ext = entry.path().extension().string();

        // Only load .so files (Linux shared libraries)
        if (ext != ".so") continue;

        GROX_LOG_DEBUG(indicator_log, "{:>20} {}", "Found plugin file", path);

        if (load_plugin(path, registry)) { loaded_count++; }
      }
    }
    catch (fs::filesystem_error const& e)
    {
      GROX_LOG_ERROR(indicator_log, "{:>20} {}", "Filesystem error", e.what());
    }

    GROX_LOG_DEBUG(indicator_log, "{:>20} {} from {}", "Plugins loaded", loaded_count, directory);

    return loaded_count;
  }

  // ----------------------------------------------------------------------------
  bool plugin_loader::load_plugin(std::string const& path, indicator_registry& registry)
  {
    loaded_plugin plugin;

    if (!load_plugin_impl(path, plugin)) { return false; }

    // Verify API version compatibility
    if ((plugin.info.api_version >> 16) != (GROX_PLUGIN_API_VERSION >> 16))
    {
      GROX_LOG_ERROR(indicator_log, "{:>20} {} requires API version {} but we have {}",
          "Plugin API mismatch", plugin.info.name, (plugin.info.api_version >> 16),
          (GROX_PLUGIN_API_VERSION >> 16));
      unload_plugin(plugin);
      return false;
    }

    // Call plugin registration function
    try
    {
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} - {}", "Registering plugin", plugin.info.name,
          plugin.info.version, plugin.info.description);

      plugin.reg_func(&registry);

      plugins_.push_back(plugin);
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} category: {}", "Plugin registered", plugin.info.name,
          plugin.info.category);
      return true;
    }
    catch (std::exception const& e)
    {
      GROX_LOG_ERROR(indicator_log, "{:>20} failed for {} : {}", "Plugin registration",
          plugin.info.name, e.what());
      unload_plugin(plugin);
      return false;
    }
  }

  // ----------------------------------------------------------------------------
  bool plugin_loader::load_plugin_impl(std::string const& path, loaded_plugin& plugin)
  {
    // Load the shared library
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
      GROX_LOG_ERROR(indicator_log, "{:>20} {} : {}", "dlopen failed", path, dlerror());
      return false;
    }

    plugin.handle = handle;
    plugin.path = path;

    // Clear any existing error
    dlerror();

    // Load the info function
    plugin.info_func =
        reinterpret_cast<grox_plugin_info_func>(dlsym(handle, "grox_plugin_get_info"));
    char const* dlsym_error = dlerror();
    if (dlsym_error || !plugin.info_func)
    {
      GROX_LOG_ERROR(
          indicator_log, "{:>20} {} : grox_plugin_get_info not found", "dlsym failed", path);
      dlclose(handle);
      return false;
    }

    // Get plugin info
    grox_plugin_info const* info_ptr = plugin.info_func();
    if (!info_ptr)
    {
      GROX_LOG_ERROR(indicator_log, "{:>20} {} : get_info returned nullptr", "Plugin info", path);
      dlclose(handle);
      return false;
    }
    plugin.info = *info_ptr;

    // Load the registration function
    plugin.reg_func =
        reinterpret_cast<grox_plugin_register_func>(dlsym(handle, "grox_plugin_register"));
    dlsym_error = dlerror();
    if (dlsym_error || !plugin.reg_func)
    {
      GROX_LOG_ERROR(
          indicator_log, "{:>20} {} : grox_plugin_register not found", "dlsym failed", path);
      dlclose(handle);
      return false;
    }

    return true;
  }

  // ----------------------------------------------------------------------------
  void plugin_loader::unload_plugin(loaded_plugin& plugin)
  {
    if (plugin.handle)
    {
      GROX_LOG_DEBUG(indicator_log, "{:>20} {}", "Unloading plugin", plugin.info.name);
      dlclose(plugin.handle);
      plugin.handle = nullptr;
    }
  }

  // ----------------------------------------------------------------------------
  void plugin_loader::unload_all()
  {
    for (auto& plugin : plugins_) { unload_plugin(plugin); }
    plugins_.clear();
  }

}    // namespace indicators
