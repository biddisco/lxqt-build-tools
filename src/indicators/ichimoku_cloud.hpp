#pragma once

#include <algorithm>
#include <deque>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Ichimoku Cloud (Ichimoku Kinko Hyo) indicator.
  ///
  /// A comprehensive indicator that defines support/resistance, identifies trend
  /// direction, gauges momentum, and provides trading signals.
  ///
  /// Components:
  ///   Tenkan-sen (Conversion Line) = (highest high + lowest low) / 2 over 9 periods
  ///   Kijun-sen  (Base Line)       = (highest high + lowest low) / 2 over 26 periods
  ///   Senkou Span A (Leading Span A) = (Tenkan + Kijun) / 2 (plotted 26 periods ahead)
  ///   Senkou Span B (Leading Span B) = (highest high + lowest low) / 2 over 52 periods
  ///                                    (plotted 26 periods ahead)
  ///   Chikou Span (Lagging Span)     = close plotted 26 periods behind (not computed here)
  ///
  /// Note: Senkou Spans are normally displaced forward, but since we process
  /// bars sequentially, we compute the current-bar values without displacement.
  /// The rendering layer can apply the shift if needed.
  ///
  /// Outputs:
  ///   [0] Tenkan-sen    (overlay: price)
  ///   [1] Kijun-sen     (overlay: price)
  ///   [2] Senkou Span A (overlay: price)
  ///   [3] Senkou Span B (overlay: price)
  class ichimoku_cloud : public indicator_base
  {
public:
    using operator_type = std::vector<float>;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(ichimoku_cloud)

    // ---------------------------------------
    /// Default constructor
    ichimoku_cloud(int tenkan_period = 9, int kijun_period = 26, int senkou_b_period = 52)
      : indicator_base("Ichimoku Cloud",
            "Ichimoku Kinko Hyo (Equilibrium chart at a glance in Japanese)",
            {overlay_type::price, overlay_type::price, overlay_type::price, overlay_type::price})
      , tenkan_period_(tenkan_period)
      , kijun_period_(kijun_period)
      , senkou_b_period_(senkou_b_period)
      , tenkan_(0)
      , kijun_(0)
      , senkou_a_(0)
      , senkou_b_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Tenkan period (Conversion Line)", 9},                    // 1
          param<int>{"Kijun period (Base Line)", 26},                          // 2
          param<int>{"Senkou B period (Leading Span B)", 52},                  // 3
      };
    }

    // ---------------------------------------
    /// four outputs: Tenkan-sen, Kijun-sen, Senkou A, Senkou B
    int num_outputs() const override { return 4; }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      tenkan_period_ = get<int>(params_, 1);
      kijun_period_ = get<int>(params_, 2);
      senkou_b_period_ = get<int>(params_, 3);
      highs_.clear();
      lows_.clear();
      tenkan_ = 0;
      kijun_ = 0;
      senkou_a_ = 0;
      senkou_b_ = 0;
    }

    // ---------------------------------------
    /// Named outputs: "tenkan", "kijun", "senkou_a", "senkou_b"
    output_descriptors get_output_descriptors() const override
    {
      return {
          {"tenkan", overlay_type::price},
          {"kijun", overlay_type::price},
          {"senkou_a", overlay_type::price},
          {"senkou_b", overlay_type::price},
      };
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& ohlc = std::get<ohlctv_sample>(sample);
      auto vals = operator()(ohlc);
      output_buffer_ = std::move(vals);
      return std::span<float const>(output_buffer_);
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& ohlc)
    {
      highs_.push_back(ohlc.high);
      lows_.push_back(ohlc.low);

      // Keep only as many bars as we need for the longest period
      int max_period = std::max({tenkan_period_, kijun_period_, senkou_b_period_});
      while (static_cast<int>(highs_.size()) > max_period)
      {
        highs_.pop_front();
        lows_.pop_front();
      }

      int n = static_cast<int>(highs_.size());

      // Tenkan-sen: mid-price over tenkan_period
      tenkan_ = midprice(std::max(0, n - tenkan_period_), n);

      // Kijun-sen: mid-price over kijun_period
      kijun_ = midprice(std::max(0, n - kijun_period_), n);

      // Senkou Span A: average of Tenkan and Kijun
      senkou_a_ = (tenkan_ + kijun_) / 2.0;

      // Senkou Span B: mid-price over senkou_b_period
      senkou_b_ = midprice(std::max(0, n - senkou_b_period_), n);

      return {static_cast<float>(tenkan_), static_cast<float>(kijun_),
          static_cast<float>(senkou_a_), static_cast<float>(senkou_b_)};
    }

    // ---------------------------------------
    inline double getLastResult() { return tenkan_; }

    inline double getTenkan() const { return tenkan_; }
    inline double getKijun() const { return kijun_; }
    inline double getSenkouA() const { return senkou_a_; }
    inline double getSenkouB() const { return senkou_b_; }

private:
    /// Compute (highest high + lowest low) / 2 over the range [from, to) in
    /// the highs_/lows_ deques
    double midprice(int from, int to) const
    {
      if (from >= to) return 0.0;
      double hh = *std::max_element(highs_.begin() + from, highs_.begin() + to);
      double ll = *std::min_element(lows_.begin() + from, lows_.begin() + to);
      return (hh + ll) / 2.0;
    }

    int tenkan_period_;
    int kijun_period_;
    int senkou_b_period_;
    std::deque<double> highs_;
    std::deque<double> lows_;
    double tenkan_;
    double kijun_;
    double senkou_a_;
    double senkou_b_;
  };

}    // namespace indicators
