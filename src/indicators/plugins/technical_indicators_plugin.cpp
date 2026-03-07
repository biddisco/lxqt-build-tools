// Technical Indicators Plugin
// Relative Strength Index, Stochastic Oscillator, Stochastic RSI,
// Average True Range, Commodity Channel Index, Rate of Change, Williams %R

#include "indicators/average_true_range.hpp"
#include "indicators/commodity_channel_index.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"
#include "indicators/rate_of_change.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"
#include "indicators/williams_percent_r.hpp"

GROX_DEFINE_PLUGIN_INFO("Technical Indicators", "1.0.0",
    "RSI, Stochastic, Stochastic RSI, ATR, CCI, ROC, and Williams %R indicators", "technical")

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

  auto atr = std::make_shared<indicators::average_true_range>();
  atr->init_params();
  registry->register_indicator(atr);

  auto cci = std::make_shared<indicators::commodity_channel_index>();
  cci->init_params();
  registry->register_indicator(cci);

  auto roc = std::make_shared<indicators::rate_of_change>();
  roc->init_params();
  registry->register_indicator(roc);

  auto wpr = std::make_shared<indicators::williams_percent_r>();
  wpr->init_params();
  registry->register_indicator(wpr);
}
GROX_END_PLUGIN_REGISTRATION()
