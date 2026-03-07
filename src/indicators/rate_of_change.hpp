#pragma once

#include <boost/circular_buffer.hpp>

#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  /// Rate of Change (ROC) indicator.
  ///
  /// Measures the percentage change in price between the current value and
  /// the value N periods ago:
  ///   ROC = ((Price - Price_N) / Price_N) * 100
  ///
  /// Positive ROC indicates upward momentum; negative indicates downward.
  /// The indicator oscillates around zero.
  ///
  /// Output: single ROC percentage value (overlay: no_overlay, separate axis)
  class rate_of_change : public indicator_base
  {
public:
    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(rate_of_change, operator_type);

    // ---------------------------------------
    /// Default constructor
    rate_of_change(int window_size = 14, ohlc_modes mode = ohlc_modes::close)
      : indicator_base("Rate of Change (ROC)", "Rate of Change", {overlay_type::no_overlay})
      , window_size_(window_size)
      , mode_(mode)
      , buffer_(window_size + 1)
      , roc_(0)
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::minute15, 5000}},    // 0
          param<int>{"Window size", 14},                                             // 1
          param<ohlc_modes>{"mode", ohlc_modes::close},                              // 2
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      buffer_ = boost::circular_buffer<double>(window_size_ + 1);
      roc_ = 0;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& ohlc)
    {
      double price = ohlc_mode_extract(mode_, ohlc);
      buffer_.push_back(price);

      if (buffer_.full())
      {
        double old_price = buffer_.front();
        if (old_price != 0.0) { roc_ = ((price - old_price) / old_price) * 100.0; }
        else { roc_ = 0.0; }
      }
      else { roc_ = 0.0; }

      return roc_;
    }

    // ---------------------------------------
    inline double getLastResult() { return roc_; }

private:
    int window_size_;
    ohlc_modes mode_;
    boost::circular_buffer<double> buffer_;
    double roc_;
  };

}    // namespace indicators
