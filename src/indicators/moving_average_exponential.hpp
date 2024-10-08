#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  struct moving_average_exponential : indicator_base
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string get_name() const override { return "Moving Average (Exponential)"; }
    const std::string get_description() const override
    {
      return "Exponential (Time-decay) Moving Average";
    }
    const overlay_type overlay = overlay_type::mode_select;

    param_list params = {
        std::make_tuple<QString, param_types>("Samples", ohlc_data_resolutions::minute15),
        std::make_tuple<QString, param_types>("Window size", 14),
        std::make_tuple<QString, param_types>("mode", ohlc_modes::close),
        std::make_tuple<QString, param_types>("User-defined alpha", false),
        std::make_tuple<QString, param_types>("Decay (1 - alpha)", 0.1),
    };

    // ---------------------------------------
    // Default constructor
    moving_average_exponential(int window_size = 14, ohlc_modes mode = ohlc_modes::low,
        bool user_alpha = false, double decay_factor = 0.1)
      : window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , user_alpha_(user_alpha)
      , decay_factor_(decay_factor)
      , first_(true)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      window_size_ = std::get<int>(std::get<1>(params[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params[2]));
      mean_ = 0.0;
      user_alpha_ = std::get<bool>(std::get<1>(params[3]));
      decay_factor_ = std::get<double>(std::get<1>(params[4]));
      first_ = true;
    }

    double operator()(ohlctv_sample const& ohlc)
    {
      double alpha = user_alpha_ ? decay_factor_ : 2.0 / (window_size_ + 1.0);
      //
      if (first_)
      {
        mean_ = ohlc_mode_extract(mode_, ohlc);
        first_ = false;
      }
      mean_ = (alpha * ohlc_mode_extract(mode_, ohlc)) + ((1.0 - alpha) * mean_);
      return mean_;
    }

    inline double getLastResult() { return mean_; }

private:
    int window_size_;
    ohlc_modes mode_;
    double mean_;
    bool user_alpha_;
    double decay_factor_;
    bool first_;
  };

}    // namespace indicators
