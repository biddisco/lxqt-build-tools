#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset.hpp"
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

    /// list of input datasets
    std::vector<ohlc_dataset*> in_datasets_;
    std::vector<point_chart_data*> out_datasets_;

public:
    /// constructor factory for a type
    template <typename Algorithm>
    static std::shared_ptr<Algorithm>
    create(Algorithm const& alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_)
    {
      // create a new instance of the algorithm
      std::shared_ptr<Algorithm> result = std::make_shared<Algorithm>();
      // copy from dialog into new instance
      *result = alg;
      // init internal structures
      result->initialize();
      // create a dataset for each indicator output
      result->create_outputs(hdf5_ohlc_);
      //
      return result;
    }

    // ----------------------------------------------------------------------------
    indicator_base(std::string const& name, std::string const& desc, overlay_type overlay)
      : name_(name)
      , description_(desc)
      , overlay_(overlay)
    {
      // indicator_dbg<2>.debug(pika::debug::detail::str<>(get_name().c_str()));
    }

    // ----------------------------------------------------------------------------
    virtual ~indicator_base()
    {
      // indicator_dbg<2>.debug(pika::debug::detail::str<>(get_name().c_str()));
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
    virtual param_list const& get_params() const { return params_; }
    virtual void set_params(param_list const& p) { params_ = p; }

    // ----------------------------------------------------------------------------
    virtual const y_limits get_ylimits() const { return {0.0, 1.0}; }

    // ----------------------------------------------------------------------------
    virtual int num_inputs() const { return 1; }
    virtual int num_outputs() const { return 1; }

    // ----------------------------------------------------------------------------
    virtual std::vector<ohlc_dataset*> const& get_input_data() const { return in_datasets_; }
    virtual std::vector<point_chart_data*>& get_output_datasets() { return out_datasets_; }

    // ----------------------------------------------------------------------------
    // create a dataset for each indicator output
    // default implementation uses first input resolution and size
    void create_outputs(std::shared_ptr<ohlc_dataset_view> view)
    {
      in_datasets_ = get_datasets(view);
      //
      const candle_res res = in_datasets_[0]->get_resolution();
      const std::size_t size = in_datasets_[0]->data().size();
      for (int i = 0; i < num_outputs(); ++i)
      {
        point_chart_data* indicator_data = new point_chart_data(res);
        indicator_data->data().reserve(size);
        out_datasets_.push_back(indicator_data);
      }
    }

    // ----------------------------------------------------------------------------
    // iterate over the parameters returned from an indicator selection dialog and
    // find the datasets of the right resolution in the datasets view
    std::vector<ohlc_dataset*> get_datasets(std::shared_ptr<ohlc_dataset_view> view)
    {
      std::vector<ohlc_dataset*> result;
      for (auto const& p : get_params())
      {
        if (candle_res const* c = std::get_if<candle_res>(&std::get<1>(p)))
        {
          result.push_back(view->get_dataset(*c));
        }
      }
      return result;
    }
  };

}    // namespace indicators
