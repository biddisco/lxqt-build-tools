#pragma once

#include <array>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  struct stochastic_oscillator
  {
    // ---------------------------------------
    // fields required for auto gui generation and plot setup
    const std::string name = "Stochastic Oscillator";
    const std::string description = "Stochastic Oscillator default 14 period";
    const overlay_type overlay = overlay_type::minmax_limit;
    const y_limits ylimits = {0.0, 1.0};

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14)};

    // ---------------------------------------
    // Default constructor
    stochastic_oscillator()
      : buffer_{}
      , stoch_val_{0.5}
      , mode_{1}
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      auto window_size = std::get<int>(std::get<1>(params[1]));
      buffer_ = boost::circular_buffer<double>(window_size);
      //auto mode = std::get<int>(std::get<1>(params[2]));
      //
      //mode_ = mode;
    }

    // ---------------------------------------
    double operator()(const double val)
    {
      buffer_.push_back(val);

      double min_val = val;
      double max_val = val;
      for (auto const& r : buffer_)
      {
        min_val = std::min(min_val, r);
        max_val = std::max(max_val, r);
      }
      if ((max_val - min_val) == 0)
      {
        stoch_val_ = 0.5;
      }
      else
        stoch_val_ = (val - min_val) / (max_val - min_val);

      return stoch_val_;
    }

    // ---------------------------------------
    inline double getLastResult()
    {
      return stoch_val_;
    }

private:
    boost::circular_buffer<double> buffer_;
    double stoch_val_;
    int mode_;
  };

}    // namespace indicators
