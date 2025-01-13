#pragma once

#include <iostream>
#include <limits>
#include <sstream>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"
#include "indicators/kernels/sliding_stop.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class trade_sell_sliding_stop : public indicator_base
  {
    enum sliding_state
    {
      rising_active,
      rising_inactive,
      falling_active,
      falling_inactive
    };

public:
    using operator_type = buy_sell_point;

    // ---------------------------------------
    FACTORY_INDICATOR_CREATE(trade_sell_sliding_stop, operator_type);

    // ---------------------------------------
    /// Default constructor
    trade_sell_sliding_stop(int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low,
        double ugap = 0.0035, double lgap = 0.0035)
      : indicator_base("Trade: Sliding Stop", "Trade: Sliding Stop",
            {
                overlay_type::buy_sell,        //
                overlay_type::buy_sell,        //
                overlay_type::buy_sell,        //
                overlay_type::relative_gain    //
            })
      , mode_(mode)
      , average_{}
      , srsi_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , fee_percent_buy_(0.2)
      , fee_percent_sell_(0.2)
      , upper_stop_(kernels::sliding_limit::up, ugap)
      , lower_stop_(kernels::sliding_limit::down, lgap)
      , gradient_(0, 0)
      , rsi_gradient_(0, 0)
    {
    }

    // ---------------------------------------
    int num_outputs() const override { return 4; }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {
          param<candle_data>{"Samples", {ohlc_data_resolutions::hour4, 1000}},    // 0
          param<int>{"Window size", 3},                                           // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_high_low},                    // 2
          param<double>{"Percentage fee Buy", 0.2},                               // 3
          param<double>{"Percentage fee Sell", 0.2},                              // 4
          param<double>{"Sliding Gap Upper", 0.15 / 100},                         // 5
          param<double>{"Gradient Threshold Upper", 0.0},                         // 6
          param<double>{"Sliding Gap Lower", 0.1 / 100},                          // 7
          param<double>{"Gradient Threshold Lower", 0.1},                         // 8
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      fee_percent_buy_ = get<double>(params_, 3);
      fee_percent_sell_ = get<double>(params_, 4);
      double gap_upper_ = get<double>(params_, 5);
      gradient_upper_ = get<double>(params_, 6);
      double gap_lower_ = get<double>(params_, 7);
      gradient_lower_ = get<double>(params_, 8);
      //
      first_ = true;
      xrp_total_ = 1;
      cash_total_ = 0;
      //
      buffer1_ = boost::circular_buffer<float>(window_size_);
      srsi_ = stochastic_relative_strength_indicator();
      average_ = moving_average_exponential_volume_weighted(window_size_, mode_);
      upper_stop_ = kernels::sliding_limit(kernels::sliding_limit::up, gap_upper_);
      lower_stop_ = kernels::sliding_limit(kernels::sliding_limit::down, gap_lower_);
      //
      param_list rsi_params_ = {                                 //
          params_[0],                                            // 0
          param<int>{"Window size", /*window_size_ * 4*/ 15},    // 1
          params_[2]};                                           // 2
      srsi_.algorithm_base::initialize(rsi_params_);
      //
      auto d1 = get_inputs()[0];
      set_time_resolution(d1.dataset_->get_resolution());
    }

    // ---------------------------------------
    void buy(double time)
    {
      double fee = 0.01 * fee_percent_buy_ * cash_total_;
      double taker_pay = cash_total_ - fee;
      //
      auto p = hdf5_ohlc_->get_trade_data_by_value(taker_pay, time + time_res_, 2.0);
      xrp_total_ = taker_pay / p.open;
      cash_total_ = 0;
      // pay the high price, value by low price
      last_result_ = {buy_sell_event_type::buy, p.open, (p.open * xrp_total_) + cash_total_,
          xrp_total_, cash_total_};
    }

    // ---------------------------------------
    void sell(double time)
    {
      double fee = 0.01 * fee_percent_sell_ * xrp_total_;
      double maker_pay = xrp_total_ - fee;
      //
      double p = hdf5_ohlc_->get_estimated_sell_price(maker_pay, time + time_res_, 2.0);
      cash_total_ = maker_pay * p;
      xrp_total_ = 0;
      // note we output the actual sell price and not the current running average
      last_result_ = {
          buy_sell_event_type::sell, p, (p * xrp_total_) + cash_total_, xrp_total_, cash_total_};
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& val)
    {
      // update the moving average filter
      double current_average_ = average_(val);
      double rsi = srsi_(val);

      if (first_)
      {
        gradient_ = kernels::gradient(current_average_, val.time);
        rsi_gradient_ = kernels::gradient(rsi, val.time);
        first_ = false;
      }
      else
      {
        gradient_(current_average_, val.time);
        rsi_gradient_(rsi, val.time);
      }

      // set default output to value with current price
      last_result_ = {buy_sell_event_type::value, current_average_,
          (val.low * xrp_total_) + cash_total_, xrp_total_, cash_total_};

      if (upper_stop_.active_)
      {
        if (!upper_stop_(current_average_) && (rsi > 0.5) && (rsi_gradient_.value() <= 0.0))
        {
          // fallen out of the upper stop range
          upper_stop_.stop();
          sell(val.time);
          lower_stop_.restart(current_average_);
        }
      }
      // else if (lower_stop_.active_)
      // {
      //   if (!lower_stop_(current_average_) && (rsi < 0.4) && (rsi_gradient_.value() > 0.0))
      //   {
      //     // fallen out of the lower stop range
      //     lower_stop_.stop();
      //     buy(val.time);
      //     upper_stop_.restart(current_average_);
      //   }
      // }

      else if ((cash_total_ > 0) && (rsi_gradient_.value() >= 0.0) && (gradient_.value() > 0.05) &&
          (rsi < 0.4))
      {
        buy(val.time);
        upper_stop_.restart(current_average_);
      }
      else if ((xrp_total_ > 0) && (rsi_gradient_.value() < 0.0) && (rsi > 0.6))
      {
        sell(val.time);
        lower_stop_.restart(current_average_);
      }
      // last_result_.value_ = rsi;
      last_result_.price_ = current_average_;
      return last_result_;
    }

    // ---------------------------------------
    inline operator_type getLastResult() { return last_result_; }

    void set_time_resolution(double res) { time_res_ = res; }

private:
    ohlc_modes mode_;
    moving_average_exponential_volume_weighted average_;
    stochastic_relative_strength_indicator srsi_;
    boost::circular_buffer<float> buffer1_;
    //
    kernels::sliding_limit upper_stop_;
    kernels::sliding_limit lower_stop_;
    kernels::gradient gradient_;
    kernels::gradient rsi_gradient_;
    //
    double gradient_upper_;
    double gradient_lower_;
    double fee_percent_buy_;
    double fee_percent_sell_;
    int window_size_;
    //
    bool first_;
    buy_sell_point last_result_;
    double xrp_total_;
    double cash_total_;
    double time_res_;
  };

}    // namespace indicators
