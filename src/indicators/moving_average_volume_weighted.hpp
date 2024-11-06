#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  struct mov_av_vw_data
  {
    double price_;
    double weight_;
  };

  //----------------------------------------------------------------------------
  class moving_average_volume_weighted : public indicator_base
  {
public:
    // ---------------------------------------
    /// Default constructor
    moving_average_volume_weighted(int window_size = 14, ohlc_modes mode = ohlc_modes::low)
      : indicator_base("Moving Average (Volume Weighted)",
            "Moving Average with Volume weighted values", overlay_type::mode_select)
      , window_size_(window_size)
      , mode_(mode)
      , mean_(0)
      , buffer_(window_size)
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
      mean_ = 0;
      //
      buffer_ = boost::circular_buffer<mov_av_vw_data>(window_size_);
    }

    // ---------------------------------------
    double compute()
    {
      double ptot = 0;
      double pwtot = 0;
      double wtot = 0;
      for (auto const& val : buffer_)
      {
        ptot += val.price_;
        pwtot += val.price_ * val.weight_;
        wtot += val.weight_;
      }
      return (wtot > 0) ? (pwtot / wtot) : ptot / buffer_.size();
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      double price = ohlc_mode_extract(mode_, val);
      buffer_.push_back({price, val.volume});
      mean_ = compute();
      return mean_;
    }

    // ---------------------------------------
    inline double getLastResult() { return mean_; }

private:
    int window_size_;
    ohlc_modes mode_;
    double mean_;
    //
    boost::circular_buffer<mov_av_vw_data> buffer_;
  };

}    // namespace indicators
