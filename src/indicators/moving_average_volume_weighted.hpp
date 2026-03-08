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
    FACTORY_INDICATOR_CREATE(moving_average_volume_weighted, operator_type);

    // ---------------------------------------
    /// Default constructor
    moving_average_volume_weighted(int window_size = 14, ohlc_modes mode = ohlc_modes::low)
      : indicator_base("Moving Average (Volume Weighted)",
            "Moving Average with Volume weighted values", {overlay_type::mode_select})
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
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
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

    int size() { return buffer_.size(); }

private:
    int window_size_;
    ohlc_modes mode_;
    double mean_;
    //
    boost::circular_buffer<mov_av_vw_data> buffer_;
  };

}    // namespace indicators
