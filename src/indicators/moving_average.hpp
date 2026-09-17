#pragma once

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

namespace indicators {

  namespace ba = boost::accumulators;

  //----------------------------------------------------------------------------
  class moving_average : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_V2(moving_average)

    // ---------------------------------------
    // Default constructor
    moving_average(int window_size = 14, ohlc_modes mode = ohlc_modes::low)
      : indicator_base("Moving Average", "Simple Moving Average", {overlay_type::mode_select})
      , window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , decay_acc_(ba::tag::rolling_window::window_size = window_size)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}},    // 0
          param<int>{"Window size", 14},                                       // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_open_close},               // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = indicators::get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      //
      decay_acc_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
          ba::tag::rolling_window::window_size = window_size_);
      mean_ = 0;
    }

    // ---------------------------------------
    /// Named output: "ma"
    output_descriptors get_output_descriptors() const override
    {
      return {{"ma", overlay_type::mode_select}};
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& ohlc = std::get<ohlctv_sample>(sample);
      double price = ohlc_mode_extract(mode_, ohlc);
      return operator()(price);
    }

    // ---------------------------------------
    /// Legacy callable interface (used by unconverted indicators)
    double operator()(double const price)
    {
      decay_acc_(price);
      mean_ = ba::rolling_mean(decay_acc_);
      return mean_;
    }

    double operator()(ohlctv_sample const& ohlc)
    {
      double price = ohlc_mode_extract(mode_, ohlc);
      return operator()(price);
    }

    // ---------------------------------------
    inline double getLastResult() { return mean_; }

private:
    int window_size_;
    ohlc_modes mode_;
    double mean_;
    //
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> decay_acc_;
  };    // namespace indicators
}    // namespace indicators
