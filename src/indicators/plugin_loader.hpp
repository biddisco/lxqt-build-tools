#pragma once

#include <memory>
#include <string>
#include <vector>
//
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"

namespace indicators {

  // Represents a loaded plugin
  struct loaded_plugin
  {
    void* handle;                          // dlopen handle
    grox_plugin_info info;                 // Plugin metadata
    std::string path;                      // Full path to plugin file
    grox_plugin_register_func reg_func;    // Registration function
    grox_plugin_info_func info_func;       // Info function
  };

  // Plugin loader class
  class plugin_loader
  {
public:
    plugin_loader() = default;
    ~plugin_loader();

    // Prevent copying
    plugin_loader(plugin_loader const&) = delete;
    plugin_loader& operator=(plugin_loader const&) = delete;

    // Load all plugins from a directory
    // Returns number of plugins successfully loaded
    std::size_t load_plugins_from_directory(
        std::string const& directory, indicator_registry& registry);

    // Load a specific plugin file
    bool load_plugin(std::string const& path, indicator_registry& registry);

    // Get list of loaded plugins
    std::vector<loaded_plugin> const& get_loaded_plugins() const { return plugins_; }

    // Unload all plugins
    void unload_all();

private:
    std::vector<loaded_plugin> plugins_;

    bool load_plugin_impl(std::string const& path, loaded_plugin& plugin);
    void unload_plugin(loaded_plugin& plugin);
  };

}    // namespace indicators
