#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_exponential.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Moving Average Convergence Divergence (MACD) indicator.
  ///
  /// The MACD is a trend-following momentum indicator that shows the relationship
  /// between two exponential moving averages of a price series.
  ///
  /// Computation:
  ///   MACD Line  = EMA(fast, default 12) - EMA(slow, default 26)
  ///   Signal Line = EMA(signal_window, default 9) of the MACD Line
  ///   Histogram   = MACD Line - Signal Line
  ///
  /// Outputs:
  ///   [0] MACD line    (overlay: no_overlay)
  ///   [1] Signal line  (overlay: no_overlay)
  ///   [2] Histogram    (overlay: no_overlay)
  class moving_average_convergence_divergence : public indicator_base
  {
public:
    using operator_type = std::vector<float>;

    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(moving_average_convergence_divergence, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_convergence_divergence(int fast_window = 12, int slow_window = 26,
        int signal_window = 9, ohlc_modes mode = ohlc_modes::close)
      : indicator_base("MACD", "Moving Average Convergence Divergence",
            {overlay_type::no_overlay, overlay_type::no_overlay, overlay_type::no_overlay})
      , fast_window_(fast_window)
      , slow_window_(slow_window)
      , signal_window_(signal_window)
      , mode_(mode)
      , ema_fast_(fast_window, mode)
      , ema_slow_(slow_window, mode)
      , macd_line_(0)
      , signal_line_(0)
      , histogram_(0)
      , signal_alpha_(2.0 / (signal_window + 1.0))
      , first_(true)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Fast window", 12},                                             // 1
          param<int>{"Slow window", 26},                                             // 2
          param<int>{"Signal window", 9},                                            // 3
          param<ohlc_modes>{"mode", ohlc_modes::close},                              // 4
      };
    }

    // ---------------------------------------
    /// three outputs: MACD line, signal line, histogram
    int num_outputs() const override { return 3; }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      fast_window_ = get<int>(params_, 1);
      slow_window_ = get<int>(params_, 2);
      signal_window_ = get<int>(params_, 3);
      mode_ = get<ohlc_modes>(params_, 4);
      //
      ema_fast_ = moving_average_exponential(fast_window_, mode_);
      ema_slow_ = moving_average_exponential(slow_window_, mode_);
      signal_alpha_ = 2.0 / (signal_window_ + 1.0);
      macd_line_ = 0;
      signal_line_ = 0;
      histogram_ = 0;
      first_ = true;
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& ohlc)
    {
      double fast_val = ema_fast_(ohlc);
      double slow_val = ema_slow_(ohlc);

      // MACD line is the difference between fast and slow EMAs
      macd_line_ = fast_val - slow_val;

      // Signal line is an EMA of the MACD line
      if (first_)
      {
        signal_line_ = macd_line_;
        first_ = false;
      }
      else { signal_line_ = (signal_alpha_ * macd_line_) + ((1.0 - signal_alpha_) * signal_line_); }

      // Histogram is the difference between MACD and signal
      histogram_ = macd_line_ - signal_line_;

      return {static_cast<float>(macd_line_), static_cast<float>(signal_line_),
          static_cast<float>(histogram_)};
    }

    // ---------------------------------------
    inline double getLastResult() { return macd_line_; }

    inline double getMACDLine() const { return macd_line_; }
    inline double getSignalLine() const { return signal_line_; }
    inline double getHistogram() const { return histogram_; }

private:
    int fast_window_;
    int slow_window_;
    int signal_window_;
    ohlc_modes mode_;
    //
    moving_average_exponential ema_fast_;
    moving_average_exponential ema_slow_;
    double macd_line_;
    double signal_line_;
    double histogram_;
    double signal_alpha_;
    bool first_;
  };

}    // namespace indicators
