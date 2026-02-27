// This file exists just to ensure that static instances of indicators are created
// and the initial vector of algorithm/indicator types is filled
#include <memory>
//
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_loader.hpp"

// Note: All indicators are now loaded as plugins at runtime.
// The plugin loader will load .so files from configured plugin directories.
namespace indicators {

  indicator_vector available_indicators;
  indicator_vector available_arbitragers;

  // ----------------------------------------------------------------------------
  indicator_registry::indicator_registry()
    : plugin_loader_(std::make_unique<plugin_loader>())
  {
  }

  // ----------------------------------------------------------------------------
  indicator_registry::~indicator_registry() = default;

  // ----------------------------------------------------------------------------
  shared_algorithm indicator_registry::find_by_name(std::string name)
  {
    auto it = std::find_if(available_indicators.begin(), available_indicators.end(),
        [&](auto it) { return it->get_name() == name; });
    if (it == available_indicators.end())
    {
      it = std::find_if(available_arbitragers.begin(), available_arbitragers.end(),
          [&](auto it) { return it->get_name() == name; });
    }
    return *it;
  }

  // ----------------------------------------------------------------------------
  std::size_t indicator_registry::load_plugins_from_directory(std::string const& directory)
  {
    return plugin_loader_->load_plugins_from_directory(directory, *this);
  }

}    // namespace indicators
