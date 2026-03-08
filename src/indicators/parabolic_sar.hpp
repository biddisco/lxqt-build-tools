#pragma once

#include <algorithm>
#include <cmath>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Parabolic SAR (Stop and Reverse) indicator.
  ///
  /// A trend-following indicator that provides potential entry and exit points.
  /// The SAR trails below price in an uptrend and above price in a downtrend.
  ///
  /// Parameters:
  ///   AF start    - initial acceleration factor (default 0.02)
  ///   AF step     - AF increment on new extreme (default 0.02)
  ///   AF maximum  - maximum acceleration factor (default 0.20)
  ///
  /// Computation:
  ///   SAR_next = SAR_current + AF * (EP - SAR_current)
  ///   where EP = extreme point (highest high in uptrend, lowest low in downtrend)
  ///   AF increases by step each time a new EP is made, capped at AF_max
  ///
  /// Output: single SAR value (overlay: price axis, plotted alongside candles)
  class parabolic_sar : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(parabolic_sar, operator_type);

    // ---------------------------------------
    /// Default constructor
    parabolic_sar(double af_start = 0.02, double af_step = 0.02, double af_max = 0.20)
      : indicator_base(
            "Parabolic SAR (Stop and Reverse)", "Parabolic Stop and Reverse", {overlay_type::price})
      , af_start_(af_start)
      , af_step_(af_step)
      , af_max_(af_max)
      , af_(af_start)
      , sar_(0)
      , ep_(0)
      , is_uptrend_(true)
      , count_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<double>{"AF start", 0.02},                                           // 1
          param<double>{"AF step", 0.02},                                            // 2
          param<double>{"AF max", 0.20},                                             // 3
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      af_start_ = get<double>(params_, 1);
      af_step_ = get<double>(params_, 2);
      af_max_ = get<double>(params_, 3);
      af_ = af_start_;
      sar_ = 0;
      ep_ = 0;
      is_uptrend_ = true;
      count_ = 0;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      if (count_ == 0)
      {
        // Initialize with first bar
        sar_ = ohlc.low;
        ep_ = ohlc.high;
        is_uptrend_ = true;
        af_ = af_start_;
        count_++;
        return sar_;
      }

      if (count_ == 1)
      {
        // Second bar: finalize initial direction
        if (ohlc.close >= sar_)
        {
          is_uptrend_ = true;
          sar_ = ohlc.low < sar_ ? ohlc.low : sar_;
          ep_ = ohlc.high;
        }
        else
        {
          is_uptrend_ = false;
          sar_ = ohlc.high;
          ep_ = ohlc.low;
        }
        count_++;
        return sar_;
      }

      // Compute next SAR
      double next_sar = sar_ + af_ * (ep_ - sar_);

      if (is_uptrend_)
      {
        // In uptrend, SAR must not be above the prior two lows
        // (we approximate with current low)
        next_sar = std::min(next_sar, ohlc.low);

        // Check for reversal: if price falls below SAR
        if (ohlc.low < next_sar)
        {
          // Reverse to downtrend
          is_uptrend_ = false;
          next_sar = ep_;    // SAR becomes the previous extreme point
          ep_ = ohlc.low;
          af_ = af_start_;
        }
        else
        {
          // Continue uptrend
          if (ohlc.high > ep_)
          {
            ep_ = ohlc.high;
            af_ = std::min(af_ + af_step_, af_max_);
          }
        }
      }
      else
      {
        // In downtrend, SAR must not be below the prior two highs
        next_sar = std::max(next_sar, ohlc.high);

        // Check for reversal: if price rises above SAR
        if (ohlc.high > next_sar)
        {
          // Reverse to uptrend
          is_uptrend_ = true;
          next_sar = ep_;    // SAR becomes the previous extreme point
          ep_ = ohlc.high;
          af_ = af_start_;
        }
        else
        {
          // Continue downtrend
          if (ohlc.low < ep_)
          {
            ep_ = ohlc.low;
            af_ = std::min(af_ + af_step_, af_max_);
          }
        }
      }

      sar_ = next_sar;
      count_++;
      return sar_;
    }

    // ---------------------------------------
    inline double getLastResult() { return sar_; }

    inline bool isUptrend() const { return is_uptrend_; }

private:
    double af_start_;
    double af_step_;
    double af_max_;
    double af_;
    double sar_;
    double ep_;
    bool is_uptrend_;
    int count_;
  };

}    // namespace indicators
