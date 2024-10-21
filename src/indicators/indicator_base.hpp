#pragma once

#include <variant>
#include <vector>

#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_utils.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
namespace indicators {

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
