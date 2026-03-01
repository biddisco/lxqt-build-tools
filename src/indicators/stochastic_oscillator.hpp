#pragma once

#include <array>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class stochastic_oscillator : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(stochastic_oscillator, operator_type);

    // ---------------------------------------
    /// Default constructor
    stochastic_oscillator()
      : indicator_base(
            "Stochastic Oscillator", "Stochastic Oscillator", {overlay_type::minmax_limit})
      , buffer_{}
      , stoch_val_{0.5}
      , mode_{1}
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Window size", 14},                                             // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_open_close},                     // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      auto window_size = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      //
      buffer_ = boost::circular_buffer<double>(window_size);
    }

    // ---------------------------------------
    double operator()(double const val)
    {
      buffer_.push_back(val);

      double min_val = val;
      double max_val = val;
      for (auto const& r : buffer_)
      {
        min_val = std::min(min_val, r);
        max_val = std::max(max_val, r);
      }
      if ((max_val - min_val) == 0) { stoch_val_ = 0.5; }
      else
        stoch_val_ = (val - min_val) / (max_val - min_val);

      return stoch_val_;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      return operator()(price);
    }

    // ---------------------------------------
    inline double getLastResult() { return stoch_val_; }

private:
    boost::circular_buffer<double> buffer_;
    double stoch_val_;
    ohlc_modes mode_;
  };

}    // namespace indicators
