#pragma once

#include <string>
#include <variant>
#include <vector>

#include "data/ohlc_data_resolutions.hpp"

namespace indicators {

  using param_types = std::variant<double, int, bool, candle_res>;
  using param_list = std::vector<std::tuple<std::string, param_types>>;

}    // namespace indicators
