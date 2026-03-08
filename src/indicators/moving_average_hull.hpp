#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_exponential.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class moving_average_hull : public indicator_base
  {
public:
    // Calculate a Weighted Moving Average with period n / 2 and multiply it by 2
    // Calculate a Weighted Moving Average for period n and subtract if from step 1
    // Calculate a Weighted Moving Average with period sqrt(n) using the data from step 2
    // HMA= WMA( 2*WMA(n/2) − WMA(n)), sqrt(n) )

    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(moving_average_hull, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_hull(int window_size = 14, ohlc_modes mode = ohlc_modes::low)
      : indicator_base("Moving Average (Hull)",
            "Moving Average using Hull formula for lowere latency", {overlay_type::mode_select})
      , window_size_(window_size)
      , mode_(mode)
      , hma_(0)
      , ema_1_(window_size / 2, mode, false, 1.0)
      , ema_2_(window_size, mode, false, 1.0)
      , ema_3_(std::sqrt(window_size), mode, false, 1.0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 14},                                       // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_open_close},               // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      hma_ = 0;
      //
      ema_1_ = moving_average_exponential(window_size_ / 2, mode_, false, 1.0);
      ema_2_ = moving_average_exponential(window_size_, mode_, false, 1.0);
      ema_3_ = moving_average_exponential(std::sqrt(window_size_), mode_, false, 1.0);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      auto v1 = ema_1_(val);
      auto v2 = ema_2_(val);
      double vwma = (2 * v1) - v2;
      hma_ = ema_3_(ohlctv_sample{val.time, vwma, vwma, vwma, vwma, val.volume});
      return hma_;
    }

    // ---------------------------------------
    inline double getLastResult() { return hma_; }

    int size() { return window_size_; }

private:
    int window_size_;
    ohlc_modes mode_;
    double hma_;
    //
    moving_average_exponential ema_1_;
    moving_average_exponential ema_2_;
    moving_average_exponential ema_3_;
  };

}    // namespace indicators
