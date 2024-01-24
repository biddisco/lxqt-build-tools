#pragma once

#include <string>
#include <variant>
#include <vector>

#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"

namespace indicators {

  using param_types = std::variant<double, int, ohlc_modes, bool, candle_res>;
  using param_list = std::vector<std::tuple<std::string, param_types>>;

  enum class overlay_type : int
  {
    price = 0,
    volume = 1,
    mode_select = 2,
    minmax_limit = 3,
    no_overlay = 4,
  };

  struct y_limits
  {
    double min;
    double max;
  };
}    // namespace indicators
