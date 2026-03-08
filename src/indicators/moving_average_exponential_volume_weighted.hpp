#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "moving_average_volume_weighted.hpp"

// Volume-weighted Exponential Moving Average (V-EMA)
// https://www.financialwebring.org/gummy-stuff/EMA.htm

namespace indicators {

  //----------------------------------------------------------------------------
  class moving_average_exponential_volume_weighted : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(moving_average_exponential_volume_weighted, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_exponential_volume_weighted(int window_size = 14,
        ohlc_modes mode = ohlc_modes::low, bool user_alpha = false, double decay_factor = 0.1)
      : indicator_base("Moving Average (Exponential, Volume Weighted)",
            "Exponential (Time-decay) Moving Average Volume Weighted", {overlay_type::mode_select})
      , window_size_(window_size)
      , mode_(mode)
      , user_alpha_(user_alpha)
      , decay_factor_(decay_factor)
      , mean_(0)
      //      , first_(true)
      , vwma_(window_size, mode)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 14},                                       // 1
          param<ohlc_modes>{"mode", ohlc_modes::close},                        // 2
          param<bool>{"User-defined alpha", false},                            // 3
          param<double>{"Decay 1 - alpha", 0.1},                               // 4
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
      //      first_ = true;
      vwma_ = moving_average_volume_weighted(window_size_, mode_);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      double vwma = vwma_(ohlc);
      double alpha = user_alpha_ ? decay_factor_ : 2.0 / (vwma_.size() + 1.0);
      //
      mean_ = (alpha * vwma) + ((1.0 - alpha) * mean_);
      return mean_;
    }

    // ---------------------------------------
    inline double getLastResult() { return mean_; }

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
