#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_exponential.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Moving Average Crossover indicator.
  /// Monitors two exponential moving averages (fast and slow) and generates
  /// a crossover signal when the fast EMA crosses above or below the slow EMA.
  /// Outputs:
  ///   [0] fast EMA value   (overlay: price)
  ///   [1] slow EMA value   (overlay: price)
  ///   [2] crossover signal (overlay: no_overlay)
  ///       +1.0 = bullish cross (fast crosses above slow)
  ///       -1.0 = bearish cross (fast crosses below slow)
  ///        0.0 = no crossover event
  class moving_average_cross : public indicator_base
  {
public:
    using operator_type = std::vector<float>;

    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(moving_average_cross, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_cross(
        int fast_window = 9, int slow_window = 21, ohlc_modes mode = ohlc_modes::close)
      : indicator_base("Moving Average Cross", "Moving Average Crossover Signal",
            {overlay_type::price, overlay_type::price, overlay_type::no_overlay})
      , fast_window_(fast_window)
      , slow_window_(slow_window)
      , mode_(mode)
      , ema_fast_(fast_window, mode)
      , ema_slow_(slow_window, mode)
      , fast_above_slow_(false)
      , first_(true)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Fast window", 9},                                              // 1
          param<int>{"Slow window", 21},                                             // 2
          param<ohlc_modes>{"mode", ohlc_modes::close},                              // 3
      };
    }

    // ---------------------------------------
    /// override outputs: fast EMA, slow EMA, crossover signal
    int num_outputs() const override { return 3; }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      fast_window_ = get<int>(params_, 1);
      slow_window_ = get<int>(params_, 2);
      mode_ = get<ohlc_modes>(params_, 3);
      //
      ema_fast_ = moving_average_exponential(fast_window_, mode_);
      ema_slow_ = moving_average_exponential(slow_window_, mode_);
      fast_above_slow_ = false;
      first_ = true;
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& ohlc)
    {
      double fast_val = ema_fast_(ohlc);
      double slow_val = ema_slow_(ohlc);

      float signal = 0.0f;

      bool currently_above = (fast_val > slow_val);
      if (first_)
      {
        // establish initial state, no signal on first sample
        fast_above_slow_ = currently_above;
        first_ = false;
      }
      else
      {
        if (currently_above && !fast_above_slow_)
        {
          // bullish crossover: fast crossed above slow
          signal = 1.0f;
        }
        else if (!currently_above && fast_above_slow_)
        {
          // bearish crossover: fast crossed below slow
          signal = -1.0f;
        }
        fast_above_slow_ = currently_above;
      }

      return {static_cast<float>(fast_val), static_cast<float>(slow_val), signal};
    }

    // ---------------------------------------
    inline double getLastResult() { return ema_fast_.getLastResult(); }

private:
    int fast_window_;
    int slow_window_;
    ohlc_modes mode_;
    //
    moving_average_exponential ema_fast_;
    moving_average_exponential ema_slow_;
    bool fast_above_slow_;
    bool first_;
  };

}    // namespace indicators
