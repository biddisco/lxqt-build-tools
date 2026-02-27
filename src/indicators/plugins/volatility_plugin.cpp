// Volatility Indicators Plugin
// Bollinger Bands, Garman-Klass, Rogers-Satchell

#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"
#include "indicators/volatility_bollinger_bands.hpp"
#include "indicators/volatility_garman_klass.hpp"
#include "indicators/volatility_rogers_satchell.hpp"

GROX_DEFINE_PLUGIN_INFO("Volatility Indicators", "1.0.0",
    "Bollinger Bands, Garman-Klass, Rogers-Satchell volatility estimators", "volatility")

GROX_BEGIN_PLUGIN_REGISTRATION()
{
  auto bb = std::make_shared<indicators::volatility_bollinger_bands>();
  bb->init_params();
  registry->register_indicator(bb);

  auto gk = std::make_shared<indicators::volatility_garman_klass>();
  gk->init_params();
  registry->register_indicator(gk);

  auto rs = std::make_shared<indicators::volatility_rogers_satchell>();
  rs->init_params();
  registry->register_indicator(rs);
}
GROX_END_PLUGIN_REGISTRATION()
