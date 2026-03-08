#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_ref.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_exponential.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Moving Average Crossover indicator.
  /// Monitors two exponential moving averages (fast and slow) and generates
  /// a crossover signal when the fast EMA crosses above or below the slow EMA.
  ///
  /// Sub-indicators are exposed as indicator_ref parameters, so the GUI
  /// automatically shows the EMA params (window size, mode, etc.) in nested
  /// group boxes — no manual param duplication required.
  ///
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
      // Create independent EMA prototypes with different default window sizes.
      // Each indicator_ref holds its own copy, so the GUI shows separate
      // parameter groups for "Fast EMA" and "Slow EMA".
      auto fast_proto = std::make_shared<moving_average_exponential>(9, ohlc_modes::close);
      fast_proto->init_params();

      auto slow_proto = std::make_shared<moving_average_exponential>(21, ohlc_modes::close);
      slow_proto->init_params();

      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},                  // 0
          param<indicator_ref>{"Fast EMA", {"Moving Average (Exponential)", fast_proto}},    // 1
          param<indicator_ref>{"Slow EMA", {"Moving Average (Exponential)", slow_proto}},    // 2
      };
    }

    // ---------------------------------------
    /// override outputs: fast EMA, slow EMA, crossover signal
    int num_outputs() const override { return 3; }

    // ---------------------------------------
    /// initialize internals from the indicator_ref parameters
    void initialize() override
    {
      // Read sub-indicator params and configure internal EMAs
      auto const& fast_ref = get<indicator_ref>(params_, 1);
      if (fast_ref.prototype_)
      {
        ema_fast_.set_params(fast_ref.prototype_->get_params());
        ema_fast_.initialize();
      }

      auto const& slow_ref = get<indicator_ref>(params_, 2);
      if (slow_ref.prototype_)
      {
        ema_slow_.set_params(slow_ref.prototype_->get_params());
        ema_slow_.initialize();
      }

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
    moving_average_exponential ema_fast_;
    moving_average_exponential ema_slow_;
    bool fast_above_slow_;
    bool first_;
  };

}    // namespace indicators
