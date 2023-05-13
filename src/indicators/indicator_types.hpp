#pragma once

#include <string>
#include <variant>
#include <vector>

#include "data/ohlc_data_resolutions.hpp"

namespace indicators {

  using param_types = std::variant<double, int, bool, candle_res>;
  using param_list = std::vector<std::tuple<std::string, param_types>>;

  inline double ohlc_mode_extract(const int mode, const QwtOHLCSample& ohlc)
  {
    switch (mode)
    {
    case 0:
      return ohlc.open;
    case 1:
      return ohlc.close;
    case 2:
      return 0.5 * (ohlc.open + ohlc.close);
    case 3:
      return ohlc.high;
    case 4:
      return ohlc.low;
    default:
      return 0.5 * (ohlc.low + ohlc.high);
    }
  }

}    // namespace indicators
