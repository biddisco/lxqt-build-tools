#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

// Volume-weighted Exponential Moving Average (V-EMA)
// https://www.financialwebring.org/gummy-stuff/EMA.htm

namespace indicators {

  struct mov_av_vw_data
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
    const std::string description = "mode : 0=open, 1=close, 2=mid(o,c), 3=high, 4=low, 5=mid(h,l)";
    const overlay_type overlay = overlay_type::mode_select;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14),
      std::make_tuple<std::string, param_types>("mode", ohlc_modes::mid_open_close)};

    // ---------------------------------------
    // Default constructor
    moving_average_volume_weighted(int window_size = 7, ohlc_modes mode = ohlc_modes::low)
      : window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , buffer_(window_size)
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      window_size_ = std::get<int>(std::get<1>(params[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params[2]));
      //
      buffer_ = boost::circular_buffer<mov_av_vw_data>(window_size_);
      mean_ = 0;
    }

    double compute()
    {
      double ptot = 0;
      double wtot = 0;
      for (auto const& val : buffer_)
      {
        ptot += val.price_ * val.weight_;
        wtot += val.weight_;
      }
      return (wtot > 0) ? (ptot / wtot) : 0.0;
    }

    double operator()(ohlctv_sample const& val)
    {
      double price = ohlc_mode_extract(ohlc_modes(mode_), val);
      // insert data into buffer
      if (val.volume > 0)
      {
        buffer_.push_back({price, val.volume});
      }
      else
      {
        auto last_vol = buffer_.back().weight_;
        buffer_.push_back({price, last_vol});
      }
      mean_ = compute();
      return mean_;
    }

    inline double getLastResult()
    {
      return mean_;
    }

private:
    int window_size_;
    ohlc_modes mode_;
    //
    boost::circular_buffer<mov_av_vw_data> buffer_;
    double mean_;
  };

}    // namespace indicators
