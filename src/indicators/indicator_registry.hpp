#pragma once

#include <memory>
#include <vector>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  // ----------------------------------------------------------------------------
  using algorithm_ptr = std::shared_ptr<algorithm_base>;
  using indicator_vector = std::vector<algorithm_ptr>;

  // declaring the indicator vector as extern helps prevent the optimizer removing
  // our initialization routine that insert each indicator type into the vector
  extern indicator_vector available_indicators;

  // Singleton registry
  class indicator_registry
  {
public:
    static indicator_registry& getInstance()
    {
      static indicator_registry instance;
      return instance;
    }

    void register_algorithm(algorithm_ptr p) { available_indicators.push_back(p); }

private:
    indicator_registry() = default;
  };

  // ----------------------------------------------------------------------------
  // Helper class to insert values into the vector, we use the redundat registry as a way of
  // preventing the optimizer from removing our insertions to a vector that appears unused
  template <typename type>
  struct indicator_type_inserter
  {
    indicator_type_inserter()
    {
      using namespace grox::debug::detail;
      using namespace grox::debug;
      indicator_dbg<0>.debug(str<>("indicator_type_inserter"), print_type<type>());
      auto p = std::make_shared<type>();
      p->init_params();
      indicator_registry::getInstance().register_algorithm(p);
    }
  };

}    // namespace indicators
