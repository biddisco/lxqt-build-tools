// This file exists just to ensure that static instances of indicators are created
// and the initial vector of algorithm/indicator types is filled
#include <cstddef>
#include <memory>
#include <vector>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/indicator_types.hpp"
//
#include "indicators/moving_average.hpp"
#include "indicators/moving_average_exponential.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/moving_average_volume_weighted.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "indicators/trade_currency_exchange.hpp"
#include "indicators/trade_sell_sliding_stop.hpp"
#include "indicators/volatility_bollinger_bands.hpp"
#include "indicators/volatility_garman_klass.hpp"
#include "indicators/volatility_rogers_satchell.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

  indicator_vector available_indicators;
  indicator_vector available_arbitragers;

  algorithm_ptr indicator_registry::find_by_name(std::string name)
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

}    // namespace indicators

static bool check_registry()
{
  auto r = indicators::indicator_registry::getInstance();
  return &r != nullptr;
}
