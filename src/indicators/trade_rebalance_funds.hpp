#pragma once

#include <iostream>
#include <limits>
#include <sstream>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "debug/logging.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"
#include "indicators/kernels/heikin_ashi.hpp"
#include "indicators/kernels/sliding_stop.hpp"
#include "indicators/moving_average_hull.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class trade_rebalance_funds : public indicator_base
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
    FACTORY_INDICATOR_CREATE(trade_rebalance_funds, operator_type);

    // ---------------------------------------
    /// Default constructor
    trade_rebalance_funds(int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low,
        double ugap = 0.0035, double lgap = 0.0035)
      : indicator_base("Trade: Rebalance funds", "Trade: Rebalance funds",
            {
                overlay_type::buy_sell,      //
                overlay_type::buy_sell,      //
                overlay_type::buy_sell,      //
                overlay_type::shared_axis    //
            })
      , mode_(mode)
      , average_{}
      , srsi_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , fee_percent_buy_(0.2)
      , fee_percent_sell_(0.2)
      , rsi_multiplier_(1.0)
      , gradient_(0, 0)
      , rsi_gradient_(0, 0)
      , time_res_{0}
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
          param<double>{"Sliding Gap Upper %", 1.0},                              // 5
          param<double>{"Sliding Gap Lower %", 1.0},                              // 6
          param<double>("RSI length multiplier", 1.0),                            // 7
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
      double gap_lower_ = get<double>(params_, 6);
      rsi_multiplier_ = get<double>(params_, 7);
      gradient_last_ = 0;
      gradient_this_ = 0;
      //
      first_ = true;
      xrp_total_ = 1;
      cash_total_ = 0;
      //
      buffer1_ = boost::circular_buffer<float>(window_size_);
      srsi_ = stochastic_relative_strength_indicator();
      average_ = moving_average_hull(window_size_, mode_);
      // upper_stop_ = kernels::sliding_limit(kernels::sliding_limit::up, gap_upper_);
      // lower_stop_ = kernels::sliding_limit(kernels::sliding_limit::down, gap_lower_);
      //
      param_list rsi_params_ = {                                                          //
          params_[0],                                                                     // 0
          param<int>{"Window size", static_cast<int>(window_size_ * rsi_multiplier_)},    // 1
          params_[2]};                                                                    // 2
      srsi_.algorithm_base::initialize(rsi_params_);
    }

    // ----------------------------------------------------------------------------
    void create_outputs(std::shared_ptr<ohlc_dataset_view> view) override
    {
      indicator_base::create_outputs(view);
      auto d1 = get_input(0);
      set_time_resolution(d1.dataset_->get_resolution());
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} {}", "set_time_resolution", get_name(),
          d1.dataset_->get_resolution().name_);
    }

    // ---------------------------------------
    void buy(double time, double cash_amount)
    {
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} {}", "buy", time, cash_amount);
      double fee = 0.01 * fee_percent_buy_ * cash_amount;
      double taker_pay = cash_amount - fee;
      //
      // auto p = hdf5_ohlc_->get_trade_data_by_value(taker_pay, time + time_res_, 2.0);
      double p = hdf5_ohlc_->get_estimated_buy_price_volume(taker_pay, time + time_res_, 2.0);
      double initial_value = (p * xrp_total_) + cash_total_;
      xrp_total_ += taker_pay / p;
      cash_total_ -= cash_amount;
      // pay the open price, (value by low price?)
      last_result_ = {//
          .event_type_ = buy_sell_event_type::buy,
          .price_ = last_result_.price_,
          .event_price_ = p,
          .value_ = (p * xrp_total_) + cash_total_,
          .tokens_ = xrp_total_,
          .cash_ = cash_total_};
      //
      assert(xrp_total_ >= 0);
      assert(cash_total_ >= 0);
      assert(initial_value >= last_result_.value_);
    }

    // ---------------------------------------
    void sell(double time, double xrp_amount)
    {
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} {}", "sell", time, xrp_amount);
      double fee = 0.01 * fee_percent_sell_ * xrp_amount;
      double maker_pay = xrp_amount - fee;
      //
      double p = hdf5_ohlc_->get_estimated_sell_price_volume(maker_pay, time + time_res_, 2.0);
      double initial_value = (p * xrp_total_) + cash_total_;

      GROX_LOG_DEBUG(indicator_log,
          "{:>20} initial_value {} cash_total_ {} xrp_total {} sell_amount {} price {}", "sell",
          initial_value, cash_total_, xrp_total_, xrp_amount, p);

      cash_total_ += maker_pay * p;
      xrp_total_ -= xrp_amount;
      // note we output the actual sell price and not the current running average
      last_result_ = {//
          .event_type_ = buy_sell_event_type::sell,
          .price_ = last_result_.price_,
          .event_price_ = p,
          .value_ = (p * xrp_total_) + cash_total_,
          .tokens_ = xrp_total_,
          .cash_ = cash_total_};
      //
      GROX_LOG_DEBUG(indicator_log,
          "{:>20} final_value {} cash_total_ {} xrp_total {} sell_amount {} price {}", "sell",
          last_result_.value_, cash_total_, xrp_total_, xrp_amount, p);
      assert(xrp_total_ >= 0);
      assert(cash_total_ >= 0);
      assert(initial_value >= last_result_.value_);
    }

    double round_n(double value, int n)
    {
      double factor = std::pow(10.0, n);
      return std::round(value * factor) / factor;
    }

    // ---------------------------------------
    void rebalance(double time, double cash_fraction)
    {
      double xrp_total_initial = xrp_total_;
      double cash_total_initial = cash_total_;
      double sell_price_est =
          hdf5_ohlc_->get_estimated_sell_price_volume(xrp_total_initial, time + time_res_, 1.0);
      double buy_price_est =
          hdf5_ohlc_->get_estimated_buy_price_volume(xrp_total_initial, time + time_res_, 1.0);
      assert(sell_price_est <= buy_price_est);
      //
      double xrp_value_sell = (sell_price_est * xrp_total_initial);
      double initial_value_est = xrp_value_sell + cash_total_initial;
      //
      if (cash_total_ < (cash_fraction * initial_value_est))
      {
        // rebalance by selling some XRP
        double excess_cash = (cash_fraction * initial_value_est) - cash_total_;
        double excess_xrp_est = excess_cash / sell_price_est;
        GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} {} {}", "rebalance-sell", time,
            initial_value_est, cash_total_, excess_xrp_est);
        sell(time, excess_xrp_est);
        if (last_result_.event_price_ < sell_price_est) sell_price_est = last_result_.event_price_;
      }
      else if (cash_total_ > cash_fraction * initial_value_est)
      {
        // rebalance by buying some XRP
        double target_cash = cash_fraction * initial_value_est;
        double excess_cash = cash_total_ - target_cash;
        GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} {} {}", "rebalance-buy", time,
            initial_value_est, cash_total_, excess_cash);
        buy(time, excess_cash);
        if (last_result_.event_price_ > buy_price_est) buy_price_est = last_result_.event_price_;
      }
      //
      initial_value_est = round_n((sell_price_est * xrp_total_initial + cash_total_initial), 6);
      double final_value_total = round_n((sell_price_est * xrp_total_) + cash_total_, 6);
      GROX_LOG_DEBUG(
          indicator_log, "{:>20} {} {}", "rebalance-value", initial_value_est, final_value_total);
      assert(final_value_total <= initial_value_est);
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& val)
    {
      // update the moving average filter
      double current_average_ = average_(val);
      double rsi = srsi_(val);

      // set default output to value computed with current price (low for conservative est)
      last_result_ = {buy_sell_event_type::value, current_average_, 0.0,
          (val.low * xrp_total_) + cash_total_, xrp_total_, cash_total_};

      if (first_)
      {
        gradient_ = kernels::gradient(current_average_, val.time);
        rsi_gradient_ = kernels::gradient(rsi, val.time);
        first_ = false;
      }
      else
      {
        gradient_(current_average_, val.time);
        gradient_this_ = rsi_gradient_(rsi, val.time);
        if (std::signbit(gradient_this_) != std::signbit(gradient_last_))
        {
          // gradient changed sign
          GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} {} {}", "gradient sign change", val.time,
              current_average_, rsi, gradient_this_);
          rebalance(val.time, rsi);
        }
      }
      gradient_last_ = gradient_this_;
      last_result_.price_ = current_average_;
      return last_result_;
    }

    // ---------------------------------------
    inline operator_type getLastResult() { return last_result_; }

    void set_time_resolution(double res) { time_res_ = res; }

private:
    ohlc_modes mode_;
    moving_average_hull average_;
    stochastic_relative_strength_indicator srsi_;
    boost::circular_buffer<float> buffer1_;
    //
    // kernels::sliding_limit upper_stop_;
    // kernels::sliding_limit lower_stop_;
    kernels::gradient gradient_;
    kernels::gradient rsi_gradient_;
    //
    double gradient_last_;
    double gradient_this_;
    double rsi_multiplier_;
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
