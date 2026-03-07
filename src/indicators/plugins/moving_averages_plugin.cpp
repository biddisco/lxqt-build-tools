// Moving Averages Plugin
// Demonstrates plugin-based indicator loading for the grox trading system

#include "indicators/indicator_registry.hpp"
#include "indicators/plugin_api.hpp"

// Include the moving average indicators
#include "indicators/moving_average.hpp"
#include "indicators/moving_average_convergence_divergence.hpp"
#include "indicators/moving_average_cross.hpp"
#include "indicators/moving_average_exponential.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/moving_average_hull.hpp"
#include "indicators/moving_average_volume_weighted.hpp"

// Define plugin metadata
GROX_DEFINE_PLUGIN_INFO("Moving Averages", "1.0.0",
    "Simple, Exponential, Volume-Weighted, Hull, Crossover, and MACD Moving Averages",
    "moving_averages")

// Register indicators with the system
GROX_BEGIN_PLUGIN_REGISTRATION()
{
  // Create and register each moving average indicator
  auto ma_simple = std::make_shared<indicators::moving_average>();
  ma_simple->init_params();
  registry->register_indicator(ma_simple);

  auto ma_exp = std::make_shared<indicators::moving_average_exponential>();
  ma_exp->init_params();
  registry->register_indicator(ma_exp);

  auto ma_exp_vw = std::make_shared<indicators::moving_average_exponential_volume_weighted>();
  ma_exp_vw->init_params();
  registry->register_indicator(ma_exp_vw);

  auto ma_hull = std::make_shared<indicators::moving_average_hull>();
  ma_hull->init_params();
  registry->register_indicator(ma_hull);

  auto ma_vw = std::make_shared<indicators::moving_average_volume_weighted>();
  ma_vw->init_params();
  registry->register_indicator(ma_vw);

  auto ma_cross = std::make_shared<indicators::moving_average_cross>();
  ma_cross->init_params();
  registry->register_indicator(ma_cross);

  auto ma_macd = std::make_shared<indicators::moving_average_convergence_divergence>();
  ma_macd->init_params();
  registry->register_indicator(ma_macd);
}
GROX_END_PLUGIN_REGISTRATION()
