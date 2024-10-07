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
using namespace grox::debug;
template <int Level>
inline print_threshold<Level, 5> indicator_dbg("Indicate");

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
    std::string name;

    virtual ~indicator_base()
    {
      // indicator_dbg<2>.debug(str<>(name.c_str()));
    }

    virtual const std::string get_name() const { return ""; }

    virtual const std::string get_description() const { return ""; }

    virtual int num_inputs() const { return 1; }
    virtual int num_outputs() const { return 1; }

    virtual overlay_type output_overlay_type(int n) const { return overlay_type::price; }

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
