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
inline grox::debug::print_threshold<Level, 5> indicator_dbg("Indicate");

// ----------------------------------------------------------------------------
namespace indicators {

  using param_types = std::variant<double, int, ohlc_modes, bool, candle_res>;
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
    volume = 1,
    /// the data might be price or volume compatible, depending on the OHLCV mode
    mode_select = 2,
    /// the data will always lie in a range (eg 0,1 for RSI etc) and is fixed Y-axis
    minmax_limit = 3,
    /// TBD
    no_overlay = 4,
  };

  // ----------------------------------------------------------------------------
  /// Used in conjunction with minmax_limit to set the y-axis range
  struct y_limits
  {
    double min;
    double max;
  };

  // ----------------------------------------------------------------------------
  class indicator_base
  {
public:
    /// by default indicators produce double precision output
    using result_type = double;

protected:
    /// generic vars that can be provided at construction time
    std::string name_;
    std::string description_;
    overlay_type overlay_;

    /// list of parameters/types that need to be supplied for GUI generation and execution
    param_list params_;

public:
    // ----------------------------------------------------------------------------
    indicator_base(const std::string& name, const std::string& desc, overlay_type overlay)
      : name_(name)
      , description_(desc)
      , overlay_(overlay)
    {
      indicator_dbg<2>.debug(pika::debug::detail::str<>(get_name().c_str()));
    }

    // ----------------------------------------------------------------------------
    virtual ~indicator_base()
    {
      indicator_dbg<2>.debug(pika::debug::detail::str<>(get_name().c_str()));
    }

    // ----------------------------------------------------------------------------
    virtual void initialize() = 0;
    virtual void init_params() = 0;

    // ----------------------------------------------------------------------------
    virtual const std::string get_name() const { return name_; }
    virtual const std::string get_description() const { return description_; }

    // ----------------------------------------------------------------------------
    /// in principle an indicator can return multiple graph series, which might require
    /// different display types, currently they are all the same, so 'n' is ignored
    virtual const overlay_type get_overlay(int n) const { return overlay_; }

    // ----------------------------------------------------------------------------
    virtual const param_list& get_params() const { return params_; }
    virtual void set_params(const param_list& p) { params_ = p; }

    // ----------------------------------------------------------------------------
    virtual const y_limits get_ylimits() const { return {0.0, 1.0}; }

    // ----------------------------------------------------------------------------
    virtual int num_inputs() const { return 1; }
    virtual int num_outputs() const { return 1; }

    // ----------------------------------------------------------------------------
    // create a dataset for each indicator output
    // default implementation uses first input resolution and size
    std::vector<point_chart_data*> create_outputs(std::vector<ohlc_dataset*> in_datasets) const
    {
      const candle_res res = in_datasets[0]->get_resolution();
      const std::size_t size = in_datasets[0]->data().size();
      std::vector<point_chart_data*> output_datasets;
      for (int i = 0; i < num_outputs(); ++i)
      {
        point_chart_data* indicator_data = new point_chart_data(res);
        indicator_data->data().reserve(size);
        output_datasets.push_back(indicator_data);
      }
      return output_datasets;
    }
  };

}    // namespace indicators
