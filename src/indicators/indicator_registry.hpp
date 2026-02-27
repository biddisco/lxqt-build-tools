#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
//
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  // Forward declaration
  class plugin_loader;

  // ----------------------------------------------------------------------------
  using indicator_vector = std::vector<shared_algorithm>;

  // Global vectors for registered indicators
  extern indicator_vector available_indicators;
  extern indicator_vector available_arbitragers;

  // Singleton registry for managing indicators and plugins
  // All indicators are loaded dynamically as plugins at runtime
  class indicator_registry
  {
public:
    static indicator_registry& getInstance()
    {
      static indicator_registry instance;
      return instance;
    }

    // Register an indicator (called by plugins during initialization)
    void register_indicator(shared_algorithm p) { available_indicators.push_back(p); }

    // Register a trading algorithm (called by plugins during initialization)
    void register_arbitrage(shared_algorithm p) { available_arbitragers.push_back(p); }

    // Find an indicator by name
    static shared_algorithm find_by_name(std::string);

    // Plugin loading support - load all plugins from a directory
    std::size_t load_plugins_from_directory(std::string const& directory);

    // Access plugin loader for advanced operations
    plugin_loader* get_plugin_loader() { return plugin_loader_.get(); }

private:
    indicator_registry();
    ~indicator_registry();

    std::unique_ptr<plugin_loader> plugin_loader_;
  };

}    // namespace indicators
