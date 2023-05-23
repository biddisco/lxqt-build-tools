#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/relative_strength_indicator.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  struct stochastic_relative_strength_indicator
  {
    // ---------------------------------------
    // fields required for auto gui generation
    const std::string name = "Stochastic RSI";
    const std::string description = "Stochastic RSI default 14 period";
    const bool price_overlay = false;

    param_list params = {
      std::make_tuple<std::string, param_types>("Samples", ohlc_data_resolutions::minute15),
      std::make_tuple<std::string, param_types>("Window size", 14)};

    // ---------------------------------------
    // Default constructor
    stochastic_relative_strength_indicator()
      : rsi_{}
      , stoch_rsi_{0}
      , mode_{1}
    {
    }

    // ---------------------------------------
    // initialize internals from a parameter list
    void initialize()
    {
      auto window_size = std::get<int>(std::get<1>(params[1]));
      buffer_ = boost::circular_buffer<double>(window_size);
      rsi_.params = params;
      rsi_.initialize();
      //auto mode = std::get<int>(std::get<1>(params[2]));
      //
      //mode_ = mode;
    }

    // ---------------------------------------
    double operator()(const QwtOHLCSample& val)
    {
      double rsi = rsi_.operator()(val);
      buffer_.push_back(rsi);

      double min_rsi = rsi;
      double max_rsi = rsi;
      for (const auto& r : buffer_)
      {
        min_rsi = std::min(min_rsi, r);
        max_rsi = std::max(max_rsi, r);
      }
      if ((max_rsi - min_rsi) == 0)
      {
        stoch_rsi_ = 0.5;
      }
      else
        stoch_rsi_ = (rsi - min_rsi) / (max_rsi - min_rsi);

      return stoch_rsi_;
    }

    // ---------------------------------------
    inline double getLastResult()
    {
      return stoch_rsi_;
    }

private:
    boost::circular_buffer<double> buffer_;
    relative_strength_indicator rsi_;
    //
    double stoch_rsi_;
    //
    int mode_;
  };

}    // namespace indicators
