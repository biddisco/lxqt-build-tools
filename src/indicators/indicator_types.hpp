#pragma once

#include <variant>
#include <vector>
//
#include <QString>

#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"

// ----------------------------------------------------------------------------
template <int Level>
inline constexpr grox::debug::print_threshold<Level, 5> indicator_dbg("Indicate");

// ----------------------------------------------------------------------------
namespace indicators {

  using param_types = std::variant<double, int, ohlc_modes, bool, candle_data>;
  using param_list = std::vector<std::tuple<QString, param_types>>;

  // ----------------------------------------------------------------------------
  /// greek symbol for sigma, used in certain indicator texts
  static constexpr QChar sigma = QChar(0xc3, 0x03);

  // ----------------------------------------------------------------------------
  /// The overlay type tells the indicator plot how/where to place the chart
  enum class overlay_type : int
  {
    /// the plot uses the same axes as the price data and can be plotted as an overlay
    price = 0,
    /// the plot uses the same axes as the volume and can be overlaid on it
    volume,
    /// the data might be price or volume compatible, depending on the OHLCV mode
    mode_select,
    /// the data will always lie in a range (eg 0,1 for RSI etc) and is fixed Y-axis
    minmax_limit,
    /// buy/sell events are plotted on the the same axes as the price data
    buy_sell,
    /// TBD
    no_overlay,
  };

  // ----------------------------------------------------------------------------
  enum class buy_sell_event_type : int
  {
    // this data point represents a buy
    buy,
    // this data point represents a sell
    sell,
    // no event takes place, but a value is returned for plotting
    value,
    // no event takes place and nothing is returned
    empty
  };

  struct buy_sell_point
  {
    buy_sell_event_type event_type_;
    double value_;
  };
  // ----------------------------------------------------------------------------
  /// Used in conjunction with minmax_limit to set the y-axis range
  struct y_limits
  {
    double min;
    double max;
  };
}    // namespace indicators
