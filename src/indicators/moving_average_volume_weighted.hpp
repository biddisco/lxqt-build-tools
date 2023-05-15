#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  struct mvwv_data
  {
    double price_;
    double weight_;
  };

  //----------------------------------------------------------------------------
  struct moving_average_volume_weighted
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Moving Average (Volume Weighted)";
    const std::string description =
      "mode : 0=open, 1=close, 2=mid(open,close), 3=high, 4=low, 5=mid(high,low)";

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 15),
      std::make_tuple<std::string, param_types>("mode", 2)};

    // ---------------------------------------
    // Default constructor (optional)
    moving_average_volume_weighted()
      : buffer_(7)
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
      buffer_ = boost::circular_buffer<mvwv_data>(window_size);
      rolling_mean_ = 0;
      mode_ = mode;
    }

    double compute()
    {
      double ptot = 0;
      double wtot = 0;
      for (const auto& val : buffer_)
      {
        ptot += val.price_ * val.weight_;
        wtot += val.weight_;
      }
      return (wtot > 0) ? (ptot / wtot) : 0.0;
    }

    double operator()(const QwtOHLCSample& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      // insert data into buffer
      buffer_.push_back({price, val.volume});
      rolling_mean_ = compute();
      return rolling_mean_;
    }

    inline double getLastResult()
    {
      return rolling_mean_;
    }

private:
    boost::circular_buffer<mvwv_data> buffer_;
    //
    double rolling_mean_;
    int mode_;
  };

}    // namespace indicators
