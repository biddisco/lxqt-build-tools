#pragma once

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
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
#include "debug/logging.hpp"
#include "indicators/indicator_ref.hpp"

// ----------------------------------------------------------------------------
inline auto indicator_log = grox::log::create("Indicate");

// ----------------------------------------------------------------------------
struct order_book_param
{
  std::string exchange_;
  currency_pair::list tickers_;
  //
  friend std::ostream& operator<<(std::ostream& os, order_book_param const& ob)
  {
    // os << "[" << ob.exchange_ << ob.ticker_ << ob.ticker_ << "]";
    return os;
  }
};

// ----------------------------------------------------------------------------
namespace indicators {

  // ---------------------------------------
  template <typename... Ts>
  struct typelist;

  using supported_types = typelist<double, int, std::string, ohlc_modes, bool, candle_data,
      order_book_param, indicator_ref>;

  // ---------------------------------------
  template <typename T>
  struct param
  {
    QString name_;
    T val_;
    //
    QString name() const { return name_; }
    void put(T val) { val_ = val; }
    T const& get() const { return val_; }
    T& get_ref() { return val_; }
    //
    friend std::ostream& operator<<(std::ostream& os, param<T> const& p)
    {
      os << p.name_.toStdString() << " " << p.get();
      return os;
    }
    // strictly we should have this friend declaration, but it triggers warnings
    // "declares a non-template function [-Wnon-template-friend]"
    // since all members are public in the struct, we can omit it for now
    // friend nlohmann::json to_json(param<T> const& p);
  };

  template <typename T>
  struct types_generator;

  template <typename... Ts>
  struct types_generator<typelist<Ts...>>
  {
    // variant with every type in the typelist
    using type = std::variant<param<Ts>...>;
  };

  using variant_type = types_generator<supported_types>::type;
  using param_list = std::vector<variant_type>;

  template <typename T>
  T const& get(param_list const& params, std::size_t i)
  {
    param<T> const* temp = std::get_if<param<T>>(&params[i]);
    return temp->get();
  }

  // // ---------------------------------------
  // void copy_from_params(param_list& params)
  // {
  //   for (param_struct& pstruct : params)
  //   {
  //     std::visit(
  //         [&pstruct](auto param) {
  //           using T = decltype(param);
  //           T* ptr = reinterpret_cast<T*>(pstruct.data_var);
  //           (*ptr) = param;
  //         },
  //         pstruct.value);
  //   }
  // }

  // // ---------------------------------------
  // param_list copy_to_params()
  // {
  //   for (param_struct& pstruct : params_)
  //   {
  //     std::visit(
  //         [&pstruct](auto param) {
  //           using T = decltype(param);
  //           T* ptr = reinterpret_cast<T*>(pstruct.data_var);
  //           (*ptr) = param;
  //         },
  //         pstruct.value);
  //   }
  //   std::get<int>(params_[1].value) = period_;
  //   std::get<int>(params_[2].value) = mode_;
  //   return params_;
  // }

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
    /// multiple outputs from the same indicator share a single axis/plot
    shared_axis,
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
    // the time of the event
    double event_time_;
    // the current tracking price of the algorithm
    double price_;
    // the price the buy/sell/trade was computed at (nominally)
    double event_price_;
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

  // ----------------------------------------------------------------------------
  /// The kind of indicator, returned by algorithm_base::kind(). Drives registry
  /// partitioning and GUI grouping. Independent from overlay_type, which is a
  /// per-output display hint only.
  enum class indicator_kind : int
  {
    /// Computes values (single or multi-series) from market samples and emits
    /// them for plotting. No portfolio state. (RSI, MA, Bollinger, ...)
    graph,
    /// Computes buy/sell signals from market samples and maintains a
    /// portfolio/paper account. (Sliding stop, rebalance funds, ...)
    strategy,
    /// Consumes order-book snapshots rather than candles. (Arbitrage, market
    /// making, ...)
    orderbook,
  };

  // ----------------------------------------------------------------------------
  /// The implementation source of an indicator, returned by
  /// algorithm_base::source(). Orthogonal to kind() — kind is about behavior,
  /// source is about implementation. Lets the GUI display an icon or filter
  /// without affecting registry partitioning.
  enum class indicator_source : int
  {
    /// Native C++ indicator, loaded as a .so plugin or built-in
    cpp,
    /// Python indicator, loaded and executed via the Python C API
    python,
  };

  // ----------------------------------------------------------------------------
  /// A point-in-time snapshot of an order book, used as the orderbook arm of
  /// market_sample. Holds copies of the bid/ask offer data so consumers can
  /// read them without holding the order book's mutex.
  struct order_book_snapshot
  {
    std::string exchange_;
    offer_data bids_;
    offer_data asks_;
    double time_;
  };

  // ----------------------------------------------------------------------------
  /// Placeholder for arbitrage/orderbook strategy decisions. Will be expanded
  /// when the orderbook indicators are fully implemented.
  struct arbitrage_decision
  {
  };

  // ----------------------------------------------------------------------------
  /// The single input type that flows into process_sample(). Every indicator
  /// handles exactly one arm; the base executes the streaming lifecycle and
  /// fans results into named outputs.
  using market_sample = std::variant<ohlctv_sample, order_book_snapshot>;

  // ----------------------------------------------------------------------------
  /// The per-sample result type returned by process_sample(). The base writes
  /// it into the indicator's outputs according to get_output_descriptors().
  ///   double             - single value per sample (RSI, MA, ...)
  ///   std::span<float const> - multiple values per sample (Bollinger, MA
  ///                        cross, ...). Points into a pre-allocated buffer
  ///                        owned by the base (sized to num_outputs() at
  ///                        initialize() time). No per-sample allocation.
  ///   buy_sell_point     - trade event with portfolio state (strategies)
  ///
  /// TODO(emit-to-sink): In a later phase, process_sample will receive an
  /// output_sink& and push named outputs directly, eliminating the return
  /// value entirely and enabling variable/sparse output counts.
  using sample_result = std::variant<double, std::span<float const>, buy_sell_point>;

  // ----------------------------------------------------------------------------
  /// Describes a single named output of an indicator. Replaces the positional
  /// outputs[N] convention; the plot layer reads outputs by name and uses
  /// overlay_type purely as a display hint.
  struct output_descriptor
  {
    std::string name_;
    overlay_type overlay_;
  };

  using output_descriptors = std::vector<output_descriptor>;
}    // namespace indicators
