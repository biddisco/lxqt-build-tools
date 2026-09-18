#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
//
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_types.hpp"
#include "logging.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  // Forward declaration
  class plugin_loader;

  // ----------------------------------------------------------------------------
  using indicator_vector = std::vector<shared_algorithm>;

  // Number of indicator_kind values (graph, strategy, orderbook)
  inline constexpr std::size_t num_kinds = 3;

  // Singleton registry for managing indicators and plugins
  // All indicators are loaded dynamically as plugins at runtime.
  // Internally partitioned by indicator_kind so consumers can request
  // a kind-filtered view via by_kind().
  class indicator_registry
  {
public:
    static indicator_registry& getInstance()
    {
      static indicator_registry instance;
      return instance;
    }

    // Register an indicator (or arbitrage/strategy). Dispatches by kind().
    void register_indicator(shared_algorithm p)
    {
      auto k = static_cast<std::size_t>(p->kind());
      GROX_LOG_TRACE(indicator_log, "{:>20} Registering [{}]: {}", "registry",
          k == 0     ? "graph" :
              k == 1 ? "strategy" :
                       "orderbook",
          p->get_name());
      indicators_by_kind_[k].push_back(p);
    }

    // Return all indicators of a given kind (graph, strategy, or orderbook).
    indicator_vector const& by_kind(indicator_kind k) const
    {
      return indicators_by_kind_[static_cast<std::size_t>(k)];
    }

    // Find an indicator by name across all kinds
    static shared_algorithm find_by_name(std::string);

    // Plugin loading support - load all plugins from a directory
    std::size_t load_plugins_from_directory(std::string const& directory);

    // Access plugin loader for advanced operations
    plugin_loader* get_plugin_loader() { return plugin_loader_.get(); }

private:
    indicator_registry();
    ~indicator_registry();

    std::array<indicator_vector, num_kinds> indicators_by_kind_;
    std::unique_ptr<plugin_loader> plugin_loader_;
  };

}    // namespace indicators
