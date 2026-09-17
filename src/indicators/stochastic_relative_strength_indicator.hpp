#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/circular_buffer.hpp>
#include <cmath>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"

namespace indicators {

  namespace ba = boost::accumulators;

  //----------------------------------------------------------------------------
  class stochastic_relative_strength_indicator : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_V2(stochastic_relative_strength_indicator)

    // ---------------------------------------
    /// Default constructor
    stochastic_relative_strength_indicator(
        int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low)
      : indicator_base(
            "Stochastic RSI", "Stochastic Relatve Strength Indicator", {overlay_type::minmax_limit})
      , window_size_(window_size)
      , mode_{mode}
      , mov_av_k_(ba::tag::rolling_window::window_size = window_size_)
      , rsi_{}
      , osc_{}
      , stoch_rsi_K{0}
      , stoch_rsi_D{0}
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      // we pass these params to rsi and osc filters, eve though we don't use all directly here
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"K smooth", 14},                                          // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_open_close},               // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mov_av_k_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
          ba::tag::rolling_window::window_size = window_size_);
      //
      rsi_.set_params(params_);
      rsi_.initialize();
      //
      osc_.set_params(params_);
      osc_.initialize();
    }

    // ---------------------------------------
    /// Named output: "stoch-rsi"
    output_descriptors get_output_descriptors() const override
    {
      return {{"stoch-rsi", overlay_type::minmax_limit}};
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& val = std::get<ohlctv_sample>(sample);
      return operator()(val);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      double rsi = rsi_.operator()(val);
      double stoch_rsi_K_unsmoothed = osc_.operator()(rsi);
      mov_av_k_(stoch_rsi_K_unsmoothed);
      stoch_rsi_K = std::round(ba::rolling_mean(mov_av_k_) * 1E6) / 1E6;
      assert(stoch_rsi_K >= 0.0 && stoch_rsi_K <= 1.0);
      return stoch_rsi_K;
    }

    // ---------------------------------------
    inline double getLastResult() { return stoch_rsi_D; }

private:
    int window_size_;
    ohlc_modes mode_;
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> mov_av_k_;
    relative_strength_indicator rsi_;
    stochastic_oscillator osc_;
    //
    double stoch_rsi_K;
    double stoch_rsi_D;
  };

}    // namespace indicators
