#pragma once
//
#include <iostream>
#include <optional>
//
#include "data/ohlc_dataset_view.hpp"
#include "data/ohlc_datasets.hpp"
#include "data/ohlc_heikin_ashi.hpp"

// Boost Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>

namespace indicators {
  using param_types = std::variant<double, int, bool, candle_res>;
  using param_list = std::vector<std::tuple<std::string, param_types>>;
  //----------------------------------------------------------------------------
  struct moving_average
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Moving Average";
    const std::string description =
      "mode : 0=open, 1=close, 2=mid(open,close), 3=high, 4=low, 5=mid(high,low)";

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 15),
      std::make_tuple<std::string, param_types>("mode", 2)};

    // ---------------------------------------
    // Default constructor required by indicator algorithms
    moving_average()
      : decay_acc_(boost::accumulators::tag::rolling_window::window_size = 7)
      , ra_(0)
      , mode_(2)
    {
    }

    // ---------------------------------------
    // Construct from parameter list required by indicator algorithms
    moving_average(const param_list& parameters)
      : params(parameters)
      , decay_acc_(boost::accumulators::tag::rolling_window::window_size =
                     std::get<int>(std::get<1>(parameters[1])))
      , ra_(0)
      , mode_(std::get<int>(std::get<1>(parameters[2])))
    {
    }

    // ---------------------------------------
    // Return a copy, constructed via a parameter list
    moving_average construct(const param_list& parameters) const
    {
      return moving_average(parameters);
    }

    double operator()(double val)
    {
      // insert data into boost accumulator
      decay_acc_(val);
      ra_ = boost::accumulators::rolling_mean(decay_acc_);
      return ra_;
    }

    inline double getLastResult()
    {
      return ra_;
    }

    void generate(std::shared_ptr<ohlc_dataset_view>&) {}

private:
    boost::accumulators::accumulator_set<double,
      boost::accumulators::stats<boost::accumulators::tag::rolling_mean>>
      decay_acc_;
    double ra_;
    int mode_;
  };
}    // namespace indicators
