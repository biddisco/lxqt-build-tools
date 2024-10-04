#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"
#include "moving_average_volume_weighted.hpp"

// Volume-weighted Exponential Moving Average (V-EMA)
// https://www.financialwebring.org/gummy-stuff/EMA.htm

namespace indicators {

  //----------------------------------------------------------------------------
  struct moving_average_exponential_volume_weighted : indicator_base
  {
    // fields required for auto gui generation
    const std::string get_name() const override
    {
      return "Moving Average (Exponential, Volume Weighted)";
    }
    const std::string get_description() const override
    {
      return "Exponential (Time-decay) Moving Average Volume Weighted";
    }
    const overlay_type overlay = overlay_type::mode_select;

    param_list params = {
      std::make_tuple<QString, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<QString, param_types>("Window size", 14),
      std::make_tuple<QString, param_types>("mode", ohlc_modes::close),
      std::make_tuple<QString, param_types>("User-defined alpha", false),
      std::make_tuple<QString, param_types>("Decay 1 - alpha", 0.1),
    };

    // ---------------------------------------
    // Default constructor
    moving_average_exponential_volume_weighted(int window_size = 14,
      ohlc_modes mode = ohlc_modes::low, bool user_alpha = false, double decay_factor = 0.1)
      : window_size_(window_size)
      , mode_(mode)
      , user_alpha_(user_alpha)
      , decay_factor_(decay_factor)
      , mean_(0)
      //      , first_(true)
      , vwma_(window_size, mode)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      window_size_ = std::get<int>(std::get<1>(params[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params[2]));
      user_alpha_ = std::get<bool>(std::get<1>(params[3]));
      decay_factor_ = std::get<double>(std::get<1>(params[4]));
      mean_ = 0.0;
      //      first_ = true;
      vwma_ = moving_average_volume_weighted(window_size_, mode_);
    }

    double operator()(ohlctv_sample const& ohlc)
    {
      double vwma = vwma_(ohlc);
      double alpha = user_alpha_ ? decay_factor_ : 2.0 / (window_size_ + 1.0);
      //
      mean_ = (alpha * vwma) + ((1.0 - alpha) * mean_);
      return mean_;
    }

    inline double getLastResult()
    {
      return mean_;
    }

private:
    int window_size_;
    ohlc_modes mode_;
    bool user_alpha_;
    double decay_factor_;
    double mean_;
    //bool first_;
    //
    moving_average_volume_weighted vwma_;
  };

}    // namespace indicators
