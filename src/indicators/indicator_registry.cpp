// This file exists just to ensure that static instances of indicators are created
// and the initial vector of algorithm/indicator types is filled
#include <algorithm>
#include <memory>
//
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_loader.hpp"

// Note: All indicators are now loaded as plugins at runtime.
// The plugin loader will load .so files from configured plugin directories.
namespace indicators {

  // ----------------------------------------------------------------------------
  indicator_registry::indicator_registry()
    : plugin_loader_(std::make_unique<plugin_loader>())
  {
  }

  // ----------------------------------------------------------------------------
  indicator_registry::~indicator_registry()
  {
    for (auto& v : indicators_by_kind_) { v.clear(); }

    // Keep explicit teardown ordering here.
    plugin_loader_.reset();
  }

  // ----------------------------------------------------------------------------
  shared_algorithm indicator_registry::find_by_name(std::string name)
  {
    auto const& by_kind = getInstance().indicators_by_kind_;
    for (auto const& v : by_kind)
    {
      auto it =
          std::find_if(v.begin(), v.end(), [&](auto const& a) { return a->get_name() == name; });
      if (it != v.end()) return *it;
    }
    return nullptr;
  }

  // ----------------------------------------------------------------------------
  std::size_t indicator_registry::load_plugins_from_directory(std::string const& directory)
  {
    return plugin_loader_->load_plugins_from_directory(directory, *this);
  }

}    // namespace indicators
