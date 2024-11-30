#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/circular_buffer.hpp>
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
    /// Default constructor
    stochastic_relative_strength_indicator()
      : indicator_base(
            "Stochastic RSI", "Stochastic Relatve Strength Indicator", {overlay_type::minmax_limit})
      , mov_av_k_(ba::tag::rolling_window::window_size = 3)
      , rsi_{}
      , osc_{}
      , stoch_rsi_K{0}
      , stoch_rsi_D{0}
      , mode_{1}
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          {"Samples", candle_data{ohlc_data_resolutions::minute15, 5000}},    //
          {"Window size", 14},                                                //
          {"K smooth", 3},
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      int k_smooth = std::get<int>(params_[2].value);
      mov_av_k_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
          ba::tag::rolling_window::window_size = k_smooth);
      //
      rsi_.set_params(params_);
      rsi_.initialize();
      //
      osc_.set_params(params_);
      osc_.initialize();
      // mode_ = std::get<int>(params_[2].value);
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      double rsi = rsi_.operator()(val);
      double stoch_rsi_K_unsmoothed = osc_.operator()(rsi);
      mov_av_k_(stoch_rsi_K_unsmoothed);
      stoch_rsi_K = ba::rolling_mean(mov_av_k_);
      return stoch_rsi_K;
    }

    // ---------------------------------------
    inline double getLastResult() { return stoch_rsi_D; }

private:
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> mov_av_k_;
    relative_strength_indicator rsi_;
    stochastic_oscillator osc_;
    //
    double stoch_rsi_K;
    double stoch_rsi_D;
    //
    int mode_;
  };

}    // namespace indicators
