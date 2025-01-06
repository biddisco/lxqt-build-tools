#pragma once

#include <iostream>
#include <variant>
#include <vector>
//
#include <QString>

#include "currency/currency.hpp"
#include "currency/currency_pair.hpp"
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"
#include "data/order_book.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"

// ----------------------------------------------------------------------------
template <int Level>
inline constexpr grox::debug::detail::print_threshold<Level, 5> indicator_dbg("Indicate");

// ----------------------------------------------------------------------------
struct order_book_param
{
  // std::shared_ptr<order_book_base> order_book_;
  std::string exchange_;
  currency_pair ticker_;
  //
  friend std::ostream& operator<<(std::ostream& os, order_book_param const& ob)
  {
    // os << "[" << ob.exchange_ << ob.ticker_ << ob.ticker_ << "]";
    return os;
  }
};

// ----------------------------------------------------------------------------
namespace indicators {

  using param_types = std::variant<double, int, ohlc_modes, bool, candle_data, order_book_param>;

  struct param_pair
  {
    QString name;
    param_types value;
  };

  using param_list = std::vector<param_pair>;

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
    /// buy/sell events are plotted on the the same axes as the price data
    buy_sell,
    /// the data will always lie in a range (eg 0,1 for RSI etc) and is fixed Y-axis
    minmax_limit,
    /// the data range from 0 to +/ some value to be found from the data
    relative_gain,
    /// TBD
    no_overlay,
  };

  using overlay_vector = std::vector<overlay_type>;

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
    // the price the trade was computed at (nominally)
    double price_;
    // the total valuation of tokens + cash
    double value_;
    // amount of cash held
    double tokens_;
    // amount of tokens held
    double cash_;
  };

  // ----------------------------------------------------------------------------
  /// Used in conjunction with minmax_limit to set the y-axis range
  struct y_limits
  {
    double min;
    double max;
  };
}    // namespace indicators
