#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_heikin_ashi.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"
#include "indicators/kernels/heikin_ashi.hpp"
#include "indicators/kernels/sliding_stop.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"
#include "indicators/portfolio.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class trade_sell_sliding_stop : public indicator_base
  {
public:
    using operator_type = buy_sell_point;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(trade_sell_sliding_stop)

    // ---------------------------------------
    /// Default constructor
    trade_sell_sliding_stop(int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low,
        double ugap = 0.0035, double lgap = 0.0035)
      : indicator_base("Trade: Sliding Stop", "Trade: Sliding Stop",
            {
                overlay_type::buy_sell,         //
                overlay_type::buy_sell,         //
                overlay_type::buy_sell,         //
                overlay_type::relative_gain,    //
                overlay_type::shared_axis,      //
            })
      , mode_(mode)
      , average_{}
      , srsi_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , upper_stop_(kernels::sliding_limit::up, ugap)
      , lower_stop_(kernels::sliding_limit::down, lgap)
      , rsi_multiplier_(1.0)
      , rsi_upper_(0.6)
      , rsi_lower_(0.2)
      , gradient_(0, 0)
      , rsi_gradient_(0, 0)
      , time_res_{0}
    {
    }

    // ---------------------------------------
    indicator_kind kind() const override { return indicator_kind::strategy; }

    // ---------------------------------------
    int num_outputs() const override { return 4; }

    // ---------------------------------------
    /// Named outputs: "buy", "sell", "price", "value"
    output_descriptors get_output_descriptors() const override
    {
      return {
          {"buy", overlay_type::buy_sell},
          {"sell", overlay_type::buy_sell},
          {"price", overlay_type::buy_sell},
          {"value", overlay_type::relative_gain},
      };
    }

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
          param<double>{"RSI Upper", 0.6},                                  // 8
          param<double>{"RSI Lower", 0.2},                                  // 9
          param<double>{"Gradient Threshold Upper", 0.0},                   // 10
          param<double>{"Gradient Threshold Lower", 0.1},                   // 11
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      portfolio_.set_fees(get<double>(params_, 3), get<double>(params_, 4));
      double gap_upper_ = get<double>(params_, 5);
      double gap_lower_ = get<double>(params_, 6);
      rsi_multiplier_ = get<double>(params_, 7);
      rsi_upper_ = get<double>(params_, 8);
      rsi_lower_ = get<double>(params_, 9);
      gradient_upper_ = get<double>(params_, 10);
      gradient_lower_ = get<double>(params_, 11);
      //
      first_ = true;
      portfolio_.reset(1.0, 0.0);
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
      portfolio_.set_time_resolution(d1.dataset_->get_resolution());
      GROX_LOG_DEBUG(indicator_log, "{:>20} {} {}", "set_time_resolution", get_name(),
          d1.dataset_->get_resolution().name_);
    }

    // ---------------------------------------
    sample_result process_sample(market_sample const& sample) override
    {
      auto const& val = std::get<ohlctv_sample>(sample);
      return operator()(val);
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& val)
    {
      // update the moving average filter
      double current_average_ = average_(val);
      double rsi = srsi_(val);

      // set default output to value computed with current price (low for conservative est)
      last_result_ = {buy_sell_event_type::value, val.time, current_average_, 0.0,
          (val.low * portfolio_.token_total()) + portfolio_.cash_total(), portfolio_.token_total(),
          portfolio_.cash_total()};

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

      if (!upper_stop_.active_ && !lower_stop_.active_)
      {
        if (portfolio_.cash_total() > 0) { lower_stop_.restart(current_average_); }
        if (portfolio_.token_total() > 0) { upper_stop_.restart(current_average_); }
      }

      if (upper_stop_.active_ && !upper_stop_(current_average_))
      {
        if ((rsi > rsi_upper_) && (rsi_gradient_.value() <= gradient_upper_))
        {
          // fallen out of the upper stop range
          upper_stop_.stop();
          last_result_ = portfolio_.sell(*hdf5_ohlc_, val.time, portfolio_.token_total());
          lower_stop_.restart(current_average_);
        }
      }
      else if (lower_stop_.active_ && !lower_stop_(current_average_))
      {
        if ((rsi < rsi_lower_) && (rsi_gradient_.value() > gradient_lower_))
        {
          // fallen out of the lower stop range
          lower_stop_.stop();
          last_result_ = portfolio_.buy(*hdf5_ohlc_, val.time, portfolio_.cash_total());
          upper_stop_.restart(current_average_);
        }
      }

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
    portfolio portfolio_;
    //
    kernels::sliding_limit upper_stop_;
    kernels::sliding_limit lower_stop_;
    kernels::gradient gradient_;
    kernels::gradient rsi_gradient_;
    kernels::heikin_ashi_transition ha_transition_;
    ohlc_heikin_ashi ha_;
    //
    double gradient_upper_;
    double gradient_lower_;
    double rsi_multiplier_;
    double rsi_upper_;
    double rsi_lower_;
    int window_size_;
    //
    bool first_;
    buy_sell_point last_result_;
    double time_res_;
  };

}    // namespace indicators
