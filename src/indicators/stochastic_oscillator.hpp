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
    /// Default constructor
    stochastic_oscillator()
      : indicator_base("Stochastic Oscillator", "Stochastic Oscillator", overlay_type::minmax_limit)
      , buffer_{}
      , stoch_val_{0.5}
      , mode_{1}
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {//
          std::make_tuple<QString, param_types>(
              "Samples", candle_data{ohlc_data_resolutions::minute15, 5000}),
          std::make_tuple<QString, param_types>("Window size", 14)};
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      auto window_size = std::get<int>(std::get<1>(params_[1]));
      buffer_ = boost::circular_buffer<double>(window_size);
      // mode_ = std::get<int>(std::get<1>(params_[2]));
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
    inline double getLastResult() { return stoch_val_; }

private:
    boost::circular_buffer<double> buffer_;
    double stoch_val_;
    int mode_;
  };

}    // namespace indicators
