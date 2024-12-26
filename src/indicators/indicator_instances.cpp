#pragma once

// This file exists just to ensure that static instances of indicators are created
// and the initial vector of algorithm/indicator types is filled

#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"
//
#include "indicators/moving_average.hpp"
#include "indicators/moving_average_exponential.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/moving_average_volume_weighted.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "indicators/trade_sell_sliding_stop.hpp"
#include "indicators/volatility_bollinger_bands.hpp"
#include "indicators/volatility_garman_klass.hpp"
#include "indicators/volatility_rogers_satchell.hpp"
