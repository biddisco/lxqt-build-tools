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
    const std::string description =
      "mode : 0=open, 1=close, 2=mid(open,close), 3=high, 4=low, 5=mid(high,low)";
    const bool price_overlay = true;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 15),
      std::make_tuple<std::string, param_types>("mode", 2)};

    // ---------------------------------------
    // Default constructor required by indicator algorithms
    moving_average()
      : decay_acc_(ba::tag::rolling_window::window_size = 7)
      , rolling_mean_(0)
      , mode_(2)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      auto window_size = std::get<int>(std::get<1>(params[1]));
      auto mode = std::get<int>(std::get<1>(params[2]));
      //
      decay_acc_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
        ba::tag::rolling_window::window_size = window_size);
      rolling_mean_ = 0;
      mode_ = mode;
    }

    double operator()(const double price)
    {
      // insert data into boost accumulator
      decay_acc_(price);
      rolling_mean_ = ba::rolling_mean(decay_acc_);
      return rolling_mean_;
    }

    double operator()(const QwtOHLCSample& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      return operator()(price);
    }

    inline double getLastResult()
    {
      return rolling_mean_;
    }

    void generate(std::shared_ptr<ohlc_dataset_view>&) {}

private:
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> decay_acc_;
    double rolling_mean_;
    int mode_;
  };
}    // namespace indicators
