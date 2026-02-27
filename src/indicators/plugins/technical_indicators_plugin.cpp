// Technical Indicators Plugin
// Relative Strength Index, Stochastic Oscillator, Stochastic RSI

#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

GROX_DEFINE_PLUGIN_INFO(
    "Technical Indicators", "1.0.0", "RSI, Stochastic, and Stochastic RSI indicators", "technical")

GROX_BEGIN_PLUGIN_REGISTRATION()
{
  auto rsi = std::make_shared<indicators::relative_strength_indicator>();
  rsi->init_params();
  registry->register_indicator(rsi);

  auto stoch = std::make_shared<indicators::stochastic_oscillator>();
  stoch->init_params();
  registry->register_indicator(stoch);

  auto stoch_rsi = std::make_shared<indicators::stochastic_relative_strength_indicator>();
  stoch_rsi->init_params();
  registry->register_indicator(stoch_rsi);
}
GROX_END_PLUGIN_REGISTRATION()
