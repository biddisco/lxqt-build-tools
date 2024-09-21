#pragma once

#include <variant>
#include <vector>
//
#include <QString>

#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"

namespace indicators {

  using param_types = std::variant<double, int, ohlc_modes, bool, candle_res>;
  using param_list = std::vector<std::tuple<QString, param_types>>;

  /// The overlay type tells the indicator plot how/where to place the chart
  enum class overlay_type : int
  {
    /// the plot uses the same axes as the price data and can be plotted as an overlay
    price = 0,
    /// the plot uses the same axes as the volume and can be overlaid on it
    volume = 1,
    /// the data might be price or volume compatible, depending on the OHLCV mode
    mode_select = 2,
    /// the data will always lie in a range (eg 0,1 for RSI etc) and is fixed Y-axis
    minmax_limit = 3,
    /// TBD
    no_overlay = 4,
  };

  /// Used in conjunction with minmax_limit to set the y-axis range
  struct y_limits
  {
    double min;
    double max;
  };

  struct indicator_base
  {
    using result_type = double;

    virtual ~indicator_base() {}

    virtual int num_inputs() const
    {
      return 1;
    }
    virtual int num_outputs() const
    {
      return 1;
    }

    virtual overlay_type output_overlay_type(int n) const
    {
      return overlay_type::price;
    }
  };

}    // namespace indicators
