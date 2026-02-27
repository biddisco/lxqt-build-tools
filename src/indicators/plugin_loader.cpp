#include "indicators/plugin_loader.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <iostream>
//
#include "debug/print.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"

namespace fs = std::filesystem;

namespace indicators {

  using namespace grox::debug;
  using pika::debug::detail::ffmt;
  using pika::debug::detail::s20;

  // ----------------------------------------------------------------------------
  plugin_loader::~plugin_loader() { unload_all(); }

  // ----------------------------------------------------------------------------
  std::size_t plugin_loader::load_plugins_from_directory(
      std::string const& directory, indicator_registry& registry)
  {
    indicator_dbg<1>.debug(ffmt<s20>("Loading plugins from"), directory);

    if (!fs::exists(directory) || !fs::is_directory(directory))
    {
      indicator_dbg<1>.warning(ffmt<s20>("Plugin directory"), "does not exist:", directory);
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

        indicator_dbg<1>.debug(ffmt<s20>("Found plugin file"), path);

        if (load_plugin(path, registry)) { loaded_count++; }
      }
    }
    catch (fs::filesystem_error const& e)
    {
      indicator_dbg<1>.error(ffmt<s20>("Filesystem error"), e.what());
    }

    indicator_dbg<1>.debug(ffmt<s20>("Plugins loaded"), loaded_count, "from", directory);

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
      indicator_dbg<1>.error(ffmt<s20>("Plugin API mismatch"), plugin.info.name,
          "requires API version", (plugin.info.api_version >> 16), "but we have",
          (GROX_PLUGIN_API_VERSION >> 16));
      unload_plugin(plugin);
      return false;
    }

    // Call plugin registration function
    try
    {
      indicator_dbg<1>.debug(ffmt<s20>("Registering plugin"), plugin.info.name, plugin.info.version,
          "-", plugin.info.description);

      plugin.reg_func(&registry);

      plugins_.push_back(plugin);
      indicator_dbg<1>.debug(
          ffmt<s20>("Plugin registered"), plugin.info.name, "category:", plugin.info.category);
      return true;
    }
    catch (std::exception const& e)
    {
      indicator_dbg<1>.error(
          ffmt<s20>("Plugin registration"), "failed for", plugin.info.name, ":", e.what());
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
      indicator_dbg<1>.error(ffmt<s20>("dlopen failed"), path, ":", dlerror());
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
      indicator_dbg<1>.error(ffmt<s20>("dlsym failed"), path, ": grox_plugin_get_info not found");
      dlclose(handle);
      return false;
    }

    // Get plugin info
    grox_plugin_info const* info_ptr = plugin.info_func();
    if (!info_ptr)
    {
      indicator_dbg<1>.error(ffmt<s20>("Plugin info"), path, ": get_info returned nullptr");
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
      indicator_dbg<1>.error(ffmt<s20>("dlsym failed"), path, ": grox_plugin_register not found");
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
      indicator_dbg<1>.debug(ffmt<s20>("Unloading plugin"), plugin.info.name);
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
