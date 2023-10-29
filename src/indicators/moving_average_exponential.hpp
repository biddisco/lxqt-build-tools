#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  struct moving_average_exponential
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Moving Average (Exponential)";
    const std::string description = "mode : 0=open, 1=close, 2=mid(o,c), 3=high, 4=low, 5=mid(h,l)";
    const bool price_overlay = true;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14),
      std::make_tuple<std::string, param_types>("mode", 1),
      std::make_tuple<std::string, param_types>("User-defined alpha", false),
      std::make_tuple<std::string, param_types>("Decay 1 - alpha", 0.1),
    };

    moving_average_exponential(int window_size = 7, double decay_factor = 0.1)
      : window_size_(window_size)
      , user_alpha_(false)
      , mode_(1)
      , decay_factor_(decay_factor)
      , xma_(0)
      , first_(true)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      window_size_ = std::get<int>(std::get<1>(params[1]));
      mode_ = std::get<int>(std::get<1>(params[2]));
      user_alpha_ = std::get<bool>(std::get<1>(params[3]));
      decay_factor_ = std::get<double>(std::get<1>(params[4]));
      xma_ = 0.0;
      first_ = true;
    }

    double operator()(QwtOHLCSample const& ohlc)
    {
      double alpha = user_alpha_ ? decay_factor_ : 2.0 / (window_size_ + 1.0);
      //
      if (first_)
      {
        xma_ = ohlc.open;
        first_ = false;
      }

      xma_ = (alpha * ohlc_mode_extract(mode_, ohlc)) + ((1.0 - alpha) * xma_);
      return xma_;
    }

    inline double getLastResult()
    {
      return xma_;
    }

private:
    int window_size_;
    bool user_alpha_;
    int mode_;
    double decay_factor_;
    double xma_;
    bool first_;
  };

}    // namespace indicators
