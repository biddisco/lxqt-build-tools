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
  struct moving_average : indicator_base
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string get_name() const override
    {
      return "Moving Average";
    }
    const std::string get_description() const override
    {
      return "Simple Moving Average";
    }
    const overlay_type overlay = overlay_type::mode_select;

    param_list params = {
      std::make_tuple<QString, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<QString, param_types>("Window size", 14),
      std::make_tuple<QString, param_types>("mode", ohlc_modes::mid_open_close)};

    // ---------------------------------------
    // Default constructor
    moving_average(int window_size = 14, ohlc_modes mode = ohlc_modes::low)
      : window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , decay_acc_(ba::tag::rolling_window::window_size = window_size)
    {
    }

    moving_average& operator=(const moving_average& other)
    {
      window_size_ = other.window_size_;
      mode_ = other.mode_;
      mean_ = other.mean_;
      decay_acc_ = other.decay_acc_;
      return *this;
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      window_size_ = std::get<int>(std::get<1>(params[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params[2]));
      //
      decay_acc_ = ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>>(
        ba::tag::rolling_window::window_size = window_size_);
      mean_ = 0;
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
      double price = ohlc_mode_extract(mode_, ohlc);
      return operator()(price);
    }

    inline double getLastResult()
    {
      return mean_;
    }

    void generate(std::shared_ptr<ohlc_dataset_view>&) {}

private:
    int window_size_;
    ohlc_modes mode_;
    double mean_;
    //
    ba::accumulator_set<double, ba::stats<ba::tag::rolling_mean>> decay_acc_;
  };
}    // namespace indicators
