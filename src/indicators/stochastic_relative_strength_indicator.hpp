#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/relative_strength_indicator.hpp"
#include "indicators/stochastic_oscillator.hpp"

namespace indicators {

  namespace ba = boost::accumulators;

  //----------------------------------------------------------------------------
  struct stochastic_relative_strength_indicator
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Stochastic RSI";
    const std::string description = "Stochastic RSI default 14 period";
    const bool price_overlay = false;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14),
      std::make_tuple<std::string, param_types>("K smooth", 3),
    };

    // ---------------------------------------
    // Default constructor
    stochastic_relative_strength_indicator()
      : mov_av_k_(ba::tag::rolling_window::window_size = 3)
      , rsi_{}
      , osc_{}
      , stoch_rsi_K{0}
      , stoch_rsi_D{0}
      , mode_{1}
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      int k_smooth = std::get<int>(std::get<1>(params[2]));
      mov_av_k_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
        ba::tag::rolling_window::window_size = k_smooth);
      //
      rsi_.params = params;
      rsi_.initialize();
      //
      osc_.params = params;
      osc_.initialize();
      //auto mode = std::get<int>(std::get<1>(params[2]));
      //
      //mode_ = mode;
    }

    // ---------------------------------------
    double operator()(const QwtOHLCSample& val)
    {
      double rsi = rsi_.operator()(val);
      double stoch_rsi_K_unsmoothed = osc_.operator()(rsi);
      mov_av_k_(stoch_rsi_K_unsmoothed);
      stoch_rsi_K = ba::rolling_mean(mov_av_k_);
      return stoch_rsi_K;
    }

    // ---------------------------------------
    inline double getLastResult()
    {
      return stoch_rsi_D;
    }

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
