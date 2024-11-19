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
      params_ = {//
          std::make_tuple<QString, param_types>(
              "Samples", candle_data{ohlc_data_resolutions::minute15, 5000}),
          std::make_tuple<QString, param_types>("Window size", 14),
          std::make_tuple<QString, param_types>("mode", ohlc_modes::mid_open_close)};
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(std::get<1>(params_[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params_[2]));
      //
      decay_acc_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
          ba::tag::rolling_window::window_size = window_size_);
      mean_ = 0;
    }

    // ---------------------------------------
    double operator()(double const price)
    {
      // insert data into boost accumulator
      decay_acc_(price);
      mean_ = ba::rolling_mean(decay_acc_);
      return mean_;
    }

    // ---------------------------------------
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
  };
}    // namespace indicators
