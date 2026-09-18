// Trading Algorithms Plugin
// Arbitrage, Market Making, and Automated Trading Strategies

#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "indicators/trade_currency_exchange.hpp"
#include "indicators/trade_market_maker.hpp"
#include "indicators/trade_rebalance_funds.hpp"
#include "indicators/trade_sell_sliding_stop.hpp"

GROX_DEFINE_PLUGIN_INFO("Trading Algorithms", "1.0.0",
    "Arbitrage, Market Making, and Automated Trading Strategies", "trading")

GROX_BEGIN_PLUGIN_REGISTRATION()
{
  auto arb_2way = std::make_shared<indicators::trade_arbitrage_2_way>();
  arb_2way->init_params();
  registry->register_indicator(arb_2way);

  auto curr_ex = std::make_shared<indicators::trade_currency_exchange>();
  curr_ex->init_params();
  registry->register_indicator(curr_ex);

  auto mm = std::make_shared<indicators::trade_market_maker>();
  mm->init_params();
  registry->register_indicator(mm);

  auto sell_stop = std::make_shared<indicators::trade_sell_sliding_stop>();
  sell_stop->init_params();
  registry->register_indicator(sell_stop);

  auto rebalance = std::make_shared<indicators::trade_rebalance_funds>();
  rebalance->init_params();
  registry->register_indicator(rebalance);
}
GROX_END_PLUGIN_REGISTRATION()
