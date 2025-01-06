#pragma once

#include <limits>
//
#include "currency/ohlctv_sample.hpp"
#include "data/ohlc_data_resolutions.hpp"

namespace indicators::kernels {

  //----------------------------------------------------------------------------
  class gradient
  {
    double prev_val_;
    double prev_time_;
    double gradient_;

public:
    gradient(double val, double time)
      : prev_val_{val}
      , prev_time_{time}
      , gradient_{0}
    {
    }
    //
    double operator()(double val, double time)
    {
      // current gradient - currency units per day (eg 1$ per day)
      double time_elapsed = (time - prev_time_) / ohlc_data_resolutions::day;
      gradient_ = (time_elapsed > 0) ? (val - prev_val_) / time_elapsed : 0;
      prev_time_ = time;
      prev_val_ = val;
      return gradient_;
    }

    inline double value() const { return gradient_; }
  };

}    // namespace indicators::kernels
