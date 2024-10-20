#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class relative_strength_indicator : public indicator_base
  {
public:
    // ---------------------------------------
    /// Default constructor
    relative_strength_indicator()
      : indicator_base("RSI", "Relatve Strength Indicator", overlay_type::minmax_limit)
      , pos_diff{0}
      , neg_diff{0}
      , count{0}
      , av_neg_d{0}
      , av_pos_d{0}
      , last_price{0}
      , period_{14}
      , rsi_{0}
      , mode_{1}
    {
    }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {//
          std::make_tuple<QString, param_types>("Samples", ohlc_data_resolutions::minute15),
          std::make_tuple<QString, param_types>("Window size", 14)};
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      auto window_size = std::get<int>(std::get<1>(params_[1]));
      //auto mode = std::get<int>(std::get<1>(params_[2]));
      //
      period_ = window_size;
      pos_diff = 0;
      neg_diff = 0;
      count = 0;
      //mode_ = mode;
    }

    // ---------------------------------------
    double operator()(double price)
    {
      double diff = price - last_price;
      last_price = price;
      //
      double pdiff = (diff > 0.0) ? diff : 0.0;
      double ndiff = (diff < 0.0) ? std::abs(diff) : 0.0;
      //
      if (count <= period_)
      {
        pos_diff += pdiff;
        neg_diff += ndiff;
        av_pos_d = pos_diff / period_;
        av_neg_d = neg_diff / period_;
        count += 1;
      }
      else if (count > period_)
      {
        av_pos_d = ((av_pos_d * (period_ - 1)) + pdiff) / period_;
        av_neg_d = ((av_neg_d * (period_ - 1)) + ndiff) / period_;
      }

      double rs;
      if (av_neg_d > 0)
        rs = av_pos_d / av_neg_d;
      else
        rs = 1.0;
      rsi_ = 1.0 - (1.0 / (1.0 + rs));

      return rsi_;
    }

    // ---------------------------------------
    double operator()(ohlctv_sample const& val)
    {
      // closing price
      double price = ohlc_mode_extract(ohlc_modes::close, val);
      return operator()(price);
    }

    // ---------------------------------------
    inline double getLastResult() { return rsi_; }

private:
    double pos_diff;
    double neg_diff;
    double count;
    double av_pos_d;
    double av_neg_d;
    double last_price;
    double period_;
    //
    double rsi_;
    int mode_;
  };

}    // namespace indicators
