// Technical Indicators Plugin
// RSI, Stochastic, Stochastic RSI, ATR, CCI, ROC, Williams %R,
// OBV, ADX, Parabolic SAR, Ichimoku Cloud, VWAP

#include "indicators/average_directional_index.hpp"
#include "indicators/average_true_range.hpp"
#include "indicators/commodity_channel_index.hpp"
#include "indicators/ichimoku_cloud.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/on_balance_volume.hpp"
#include "indicators/parabolic_sar.hpp"
#include "indicators/plugin_api.hpp"
#include "indicators/rate_of_change.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"
#include "indicators/volume_weighted_average_price.hpp"
#include "indicators/williams_percent_r.hpp"

GROX_DEFINE_PLUGIN_INFO("Technical Indicators", "1.0.0",
    "RSI, Stochastic, StochRSI, ATR, CCI, ROC, Williams%R, OBV, ADX, PSAR, Ichimoku, VWAP",
    "technical")

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

  auto obv = std::make_shared<indicators::on_balance_volume>();
  obv->init_params();
  registry->register_indicator(obv);

  auto adx = std::make_shared<indicators::average_directional_index>();
  adx->init_params();
  registry->register_indicator(adx);

  auto psar = std::make_shared<indicators::parabolic_sar>();
  psar->init_params();
  registry->register_indicator(psar);

  auto ichimoku = std::make_shared<indicators::ichimoku_cloud>();
  ichimoku->init_params();
  registry->register_indicator(ichimoku);

  auto vwap = std::make_shared<indicators::volume_weighted_average_price>();
  vwap->init_params();
  registry->register_indicator(vwap);
}
GROX_END_PLUGIN_REGISTRATION()
