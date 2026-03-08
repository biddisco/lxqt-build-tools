#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class moving_average_exponential : public indicator_base
  {
    // Window Mapping (Approximation):
    // To convert an EMA to a linear moving average, align the time frames using the relation
    // alpha = 2/(N+1), where N is the number of periods in a simple moving average.

public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(moving_average_exponential, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_exponential(int window_size = 14, ohlc_modes mode = ohlc_modes::low,
        bool user_alpha = false, double decay_factor = 0.1)
      : indicator_base("Moving Average (Exponential)", "Exponential (Time-decay) Moving Average",
            {overlay_type::mode_select})
      , window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , user_alpha_(user_alpha)
      , decay_factor_(decay_factor)
      , first_(true)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", window_size_},                             // 1
          param<ohlc_modes>{"mode", mode_},                                    // 2
          param<bool>{"User-defined alpha", user_alpha_},                      // 3
          param<double>{"Decay (1 - alpha)", decay_factor_},                   // 4
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      user_alpha_ = get<bool>(params_, 3);
      decay_factor_ = get<double>(params_, 4);
      mean_ = 0.0;
      first_ = true;
    }

    // ---------------------------------------
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

    // ---------------------------------------
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
