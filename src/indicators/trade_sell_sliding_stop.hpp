#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"
#include "indicators/kernels/heikin_ashi.hpp"
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
          param<candle_data>{"Samples", {ohlc_data_resolutions::hour4}},    // 0
          param<int>{"Window size", 3},                                     // 1
          param<ohlc_modes>{"mode", ohlc_modes::mid_high_low},              // 2
          param<double>{"Percentage fee Buy", 0.2},                         // 3
          param<double>{"Percentage fee Sell", 0.2},                        // 4
          param<double>{"Sliding Gap Upper %", 22.0},                       // 5
          param<double>{"Sliding Gap Lower %", 0.0},                        // 6
          param<double>{"RSI length multiplier", 10.0},                     // 7
          param<double>{"Gradient Threshold Upper", 0.0},                   // 8
          param<double>{"Gradient Threshold Lower", 0.1},                   // 9
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
      gradient_upper_ = get<double>(params_, 8);
      gradient_lower_ = get<double>(params_, 9);
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
      double p = hdf5_ohlc_->get_estimated_buy_price_value(cash_amount, time + time_res_, 2.0);
      double initial_value = (p * xrp_total_) + cash_total_;
      //
      double fee = 0.01 * fee_percent_buy_ * cash_amount;
      double taker_pay = cash_amount - fee;
      double xrp_bought = taker_pay / p;
      //
      xrp_total_ += xrp_bought;
      cash_total_ -= cash_amount;
      //
      GROX_LOG_DEBUG(indicator_log,
          "{:>20} bought {:.2f} XRP at price {:.2f} for total {:.2f} with fee {:.2f}", "buy",
          xrp_bought, p, taker_pay, fee);
      GROX_LOG_DEBUG(indicator_log,
          "{:>20} initial_value {:.2f} final_value {:.2f} cash_total_ {:.2f} xrp_total {:.2f}",
          "buy", initial_value, last_result_.value_, cash_total_, xrp_total_);

      last_result_ = {//
          .event_type_ = buy_sell_event_type::buy,
          .event_time_ = time + time_res_,
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
      double p = hdf5_ohlc_->get_estimated_sell_price_volume(xrp_amount, time + time_res_, 2.0);
      double initial_value = (p * xrp_total_) + cash_total_;

      double taker_pay = xrp_amount * p;
      double fee = 0.01 * fee_percent_sell_ * taker_pay;
      cash_total_ += taker_pay - fee;
      xrp_total_ -= xrp_amount;
      //
      GROX_LOG_DEBUG(indicator_log,
          "{:>20} sold {:.2f} XRP at price {:.2f} for total {:.2f} with fee {:.2f}", "sell",
          xrp_amount, p, taker_pay, fee);
      GROX_LOG_DEBUG(indicator_log,
          "{:>20} initial_value {:.2f} final_value {:.2f} cash_total_ {:.2f} xrp_total {:.2f}",
          "sell", initial_value, last_result_.value_, cash_total_, xrp_total_);

      // note we output the actual sell price and not the current running average
      last_result_ = {//
          .event_type_ = buy_sell_event_type::sell,
          .event_time_ = time + time_res_,
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
    operator_type operator()(ohlctv_sample const& val)
    {
      // update the moving average filter
      double current_average_ = average_(val);
      double rsi = srsi_(val);

      // set default output to value computed with current price (low for conservative est)
      last_result_ = {buy_sell_event_type::value, val.time, current_average_, 0.0,
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
        rsi_gradient_(rsi, val.time);
      }

#if 1
      if (!upper_stop_.active_ && !lower_stop_.active_)
      {
        if (cash_total_ > 0) { lower_stop_.restart(current_average_); }
        if (xrp_total_ > 0) { upper_stop_.restart(current_average_); }
      }

      if (upper_stop_.active_ && !upper_stop_(current_average_))
      {
        if ((rsi > 0.6) && (rsi_gradient_.value() <= gradient_upper_))
        {
          // fallen out of the upper stop range
          upper_stop_.stop();
          sell(val.time, xrp_total_);
          lower_stop_.restart(current_average_);
        }
      }
      else if (lower_stop_.active_ && !lower_stop_(current_average_))
      {
        if ((rsi < 0.2) && (rsi_gradient_.value() > gradient_lower_))
        {
          // fallen out of the lower stop range
          lower_stop_.stop();
          buy(val.time, cash_total_);
          upper_stop_.restart(current_average_);
        }
      }
#else
      auto ha_event = ha_transition_(val);

      if (ha_event == buy_sell_event_type::buy && (cash_total_ > 0))
      {    //
        buy(val.time, cash_total_);
      }
      else if (ha_event == buy_sell_event_type::sell && (xrp_total_ > 0))
      {    //
        sell(val.time, xrp_total_);
      }
#endif
      last_result_.price_ = current_average_;
      return last_result_;
    }

    // ---------------------------------------
    inline operator_type getLastResult() { return last_result_; }

    // ---------------------------------------
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
    kernels::heikin_ashi_transition ha_transition_;
    //
    double gradient_upper_;
    double gradient_lower_;
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
