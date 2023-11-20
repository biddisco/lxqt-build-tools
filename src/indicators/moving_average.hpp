#pragma once

#include "data/ohlc_data_resolutions.hpp"
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
  struct moving_average
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Moving Average";
    const std::string description = "mode : 0=open, 1=close, 2=mid(o,c), 3=high, 4=low, 5=mid(h,l)";
    const overlay_type overlay = overlay_type::mode_select;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14),
      std::make_tuple<std::string, param_types>("mode", ohlc_modes::mid_open_close)};

    // ---------------------------------------
    // Default constructor required by indicator algorithms
    moving_average()
      : decay_acc_(ba::tag::rolling_window::window_size = 14)
      , mode_(ohlc_modes::low)
      , mean_(0)
    {
    }

    moving_average(int window_size, ohlc_modes mode)
      : decay_acc_(ba::tag::rolling_window::window_size = window_size)
      , mode_(mode)
      , mean_(0.0)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      auto window_size = std::get<int>(std::get<1>(params[1]));
      auto mode = std::get<ohlc_modes>(std::get<1>(params[2]));
      //
      decay_acc_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
        ba::tag::rolling_window::window_size = window_size);
      mean_ = 0;
      mode_ = mode;
    }

    double operator()(const double price)
    {
      // insert data into boost accumulator
      decay_acc_(price);
      mean_ = ba::rolling_mean(decay_acc_);
      return mean_;
    }

    double operator()(ohlctv_sample const& ohlc)
    {
      double price = ohlc_mode_extract(ohlc_modes(mode_), ohlc);
      return operator()(price);
    }

    inline double getLastResult()
    {
      return mean_;
    }

    void generate(std::shared_ptr<ohlc_dataset_view>&) {}

private:
    ohlc_modes mode_;
    double mean_;
    //
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> decay_acc_;
  };
}    // namespace indicators
