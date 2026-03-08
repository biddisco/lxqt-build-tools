#pragma once

#include <cmath>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// On-Balance Volume (OBV) indicator.
  ///
  /// A cumulative volume-based momentum indicator that relates volume to price
  /// change. OBV rises when volume on up-days exceeds volume on down-days,
  /// and falls when the reverse is true.
  ///
  /// Rules:
  ///   If close > previous close: OBV = OBV_prev + volume
  ///   If close < previous close: OBV = OBV_prev - volume
  ///   If close == previous close: OBV = OBV_prev
  ///
  /// An optional EMA smoothing length can be applied to the raw OBV.
  /// When smoothing_length > 1, the output is the EMA of the raw OBV.
  /// When smoothing_length <= 1 (default), the raw OBV is returned unsmoothed.
  ///
  /// When use_gradient is true, the output is the gradient (rate of change per
  /// day) of the OBV rather than its absolute value. This uses the gradient
  /// kernel from indicators/kernels/gradient.hpp.
  ///
  /// Output: single OBV value (overlay: volume axis)
  class on_balance_volume : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(on_balance_volume, operator_type);

    // ---------------------------------------
    /// Default constructor
    on_balance_volume(int smoothing_length = 1, bool use_gradient = false)
      : indicator_base("On-Balance Volume (OBV)", "On-Balance Volume", {overlay_type::volume})
      , smoothing_length_(smoothing_length)
      , use_gradient_(use_gradient)
      , ema_alpha_(smoothing_length > 1 ? 2.0 / (smoothing_length + 1.0) : 1.0)
      , obv_(0)
      , smoothed_obv_(0)
      , prev_close_(0)
      , first_(true)
      , first_smooth_(true)
      , gradient_(0, 0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Smoothing length", 1},                                   // 1
          param<bool>{"Use gradient", false},                                  // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      smoothing_length_ = get<int>(params_, 1);
      use_gradient_ = get<bool>(params_, 2);
      ema_alpha_ = (smoothing_length_ > 1) ? 2.0 / (smoothing_length_ + 1.0) : 1.0;
      obv_ = 0;
      smoothed_obv_ = 0;
      prev_close_ = 0;
      first_ = true;
      first_smooth_ = true;
      gradient_ = kernels::gradient(0, 0);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      if (first_)
      {
        obv_ = 0;
        prev_close_ = ohlc.close;
        first_ = false;
        gradient_ = kernels::gradient(obv_, ohlc.time);
      }
      else
      {
        if (ohlc.close > prev_close_) { obv_ += ohlc.volume; }
        else if (ohlc.close < prev_close_) { obv_ -= ohlc.volume; }
        // if close == prev_close, OBV unchanged
        prev_close_ = ohlc.close;
      }

      // Apply optional EMA smoothing
      double result = obv_;
      if (smoothing_length_ > 1)
      {
        if (first_smooth_)
        {
          smoothed_obv_ = obv_;
          first_smooth_ = false;
        }
        else { smoothed_obv_ = ema_alpha_ * obv_ + (1.0 - ema_alpha_) * smoothed_obv_; }
        result = smoothed_obv_;
      }

      // Optionally return the gradient instead of the absolute value
      if (use_gradient_) { result = gradient_(result, ohlc.time); }

      return result;
    }

    // ---------------------------------------
    inline double getLastResult()
    {
      if (use_gradient_) { return gradient_.value(); }
      return (smoothing_length_ > 1) ? smoothed_obv_ : obv_;
    }

private:
    int smoothing_length_;
    bool use_gradient_;
    double ema_alpha_;
    double obv_;
    double smoothed_obv_;
    double prev_close_;
    bool first_;
    bool first_smooth_;
    kernels::gradient gradient_;
  };

}    // namespace indicators
