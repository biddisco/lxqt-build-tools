#pragma once

#include <algorithm>
#include <cmath>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Average Directional Index (ADX) indicator.
  ///
  /// Measures trend strength regardless of direction. Consists of three lines:
  ///   +DI (positive directional indicator) - measures upward trend strength
  ///   -DI (negative directional indicator) - measures downward trend strength
  ///   ADX - smoothed average of the absolute difference between +DI and -DI
  ///         divided by their sum, indicating overall trend strength
  ///
  /// Computation (Wilder's smoothing with period N):
  ///   +DM = max(High - prevHigh, 0) if > max(prevLow - Low, 0), else 0
  ///   -DM = max(prevLow - Low, 0) if > max(High - prevHigh, 0), else 0
  ///   TR  = max(H-L, |H-prevC|, |L-prevC|)
  ///   Smoothed +DM, -DM, TR using Wilder's method
  ///   +DI = 100 * smoothed(+DM) / smoothed(TR)
  ///   -DI = 100 * smoothed(-DM) / smoothed(TR)
  ///   DX  = 100 * |+DI - -DI| / (+DI + -DI)
  ///   ADX = Wilder's smoothed DX
  ///
  /// Outputs:
  ///   [0] ADX  (overlay: shared_axis)
  ///   [1] +DI  (overlay: shared_axis)
  ///   [2] -DI  (overlay: shared_axis)
  class average_directional_index : public indicator_base
  {
public:
    using operator_type = std::vector<float>;

    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(average_directional_index, operator_type);

    // ---------------------------------------
    /// Default constructor
    average_directional_index(int window_size = 14)
      : indicator_base("ADX", "Average Directional Index",
            {overlay_type::shared_axis, overlay_type::shared_axis, overlay_type::shared_axis})
      , window_size_(window_size)
      , smoothed_plus_dm_(0)
      , smoothed_minus_dm_(0)
      , smoothed_tr_(0)
      , plus_di_(0)
      , minus_di_(0)
      , adx_(0)
      , prev_high_(0)
      , prev_low_(0)
      , prev_close_(0)
      , count_(0)
      , first_(true)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Window size", 14},                                             // 1
      };
    }

    // ---------------------------------------
    /// three outputs: +DI, -DI, ADX
    int num_outputs() const override { return 3; }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      smoothed_plus_dm_ = 0;
      smoothed_minus_dm_ = 0;
      smoothed_tr_ = 0;
      plus_di_ = 0;
      minus_di_ = 0;
      adx_ = 0;
      prev_high_ = 0;
      prev_low_ = 0;
      prev_close_ = 0;
      count_ = 0;
      first_ = true;
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& ohlc)
    {
      if (first_)
      {
        prev_high_ = ohlc.high;
        prev_low_ = ohlc.low;
        prev_close_ = ohlc.close;
        first_ = false;
        return {0.0f, 0.0f, 0.0f};    // {ADX, +DI, -DI}
      }

      // Directional Movement
      double up_move = ohlc.high - prev_high_;
      double down_move = prev_low_ - ohlc.low;

      double plus_dm = 0;
      double minus_dm = 0;
      if (up_move > down_move && up_move > 0) { plus_dm = up_move; }
      if (down_move > up_move && down_move > 0) { minus_dm = down_move; }

      // True Range
      double hl = ohlc.high - ohlc.low;
      double hpc = std::abs(ohlc.high - prev_close_);
      double lpc = std::abs(ohlc.low - prev_close_);
      double tr = std::max({hl, hpc, lpc});

      prev_high_ = ohlc.high;
      prev_low_ = ohlc.low;
      prev_close_ = ohlc.close;

      // Wilder's smoothing
      if (count_ < window_size_)
      {
        smoothed_plus_dm_ += plus_dm;
        smoothed_minus_dm_ += minus_dm;
        smoothed_tr_ += tr;
        count_++;

        if (count_ == window_size_)
        {
          // First full period: use simple sum then compute DI
          if (smoothed_tr_ != 0.0)
          {
            plus_di_ = 100.0 * smoothed_plus_dm_ / smoothed_tr_;
            minus_di_ = 100.0 * smoothed_minus_dm_ / smoothed_tr_;
          }
          double di_sum = plus_di_ + minus_di_;
          double dx = (di_sum != 0.0) ? 100.0 * std::abs(plus_di_ - minus_di_) / di_sum : 0.0;
          adx_ = dx;
        }
      }
      else
      {
        // Wilder's smoothing: smoothed = prev - (prev/N) + current
        smoothed_plus_dm_ = smoothed_plus_dm_ - (smoothed_plus_dm_ / window_size_) + plus_dm;
        smoothed_minus_dm_ = smoothed_minus_dm_ - (smoothed_minus_dm_ / window_size_) + minus_dm;
        smoothed_tr_ = smoothed_tr_ - (smoothed_tr_ / window_size_) + tr;

        if (smoothed_tr_ != 0.0)
        {
          plus_di_ = 100.0 * smoothed_plus_dm_ / smoothed_tr_;
          minus_di_ = 100.0 * smoothed_minus_dm_ / smoothed_tr_;
        }

        double di_sum = plus_di_ + minus_di_;
        double dx = (di_sum != 0.0) ? 100.0 * std::abs(plus_di_ - minus_di_) / di_sum : 0.0;

        // ADX = Wilder's smoothed DX
        adx_ = ((adx_ * (window_size_ - 1)) + dx) / window_size_;
      }

      return {
          static_cast<float>(adx_), static_cast<float>(plus_di_), static_cast<float>(minus_di_)};
    }

    // ---------------------------------------
    inline double getLastResult() { return adx_; }

    inline double getPlusDI() const { return plus_di_; }
    inline double getMinusDI() const { return minus_di_; }
    inline double getADX() const { return adx_; }

private:
    int window_size_;
    double smoothed_plus_dm_;
    double smoothed_minus_dm_;
    double smoothed_tr_;
    double plus_di_;
    double minus_di_;
    double adx_;
    double prev_high_;
    double prev_low_;
    double prev_close_;
    int count_;
    bool first_;
  };

}    // namespace indicators
