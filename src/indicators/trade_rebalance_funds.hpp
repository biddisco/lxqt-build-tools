#pragma once

#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "debug/logging.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/kernels/gradient.hpp"
#include "indicators/moving_average_hull.hpp"
#include "indicators/portfolio.hpp"
#include "indicators/stochastic_relative_strength_indicator.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class trade_rebalance_funds : public indicator_base
  {
public:
    using operator_type = buy_sell_point;

    // ---------------------------------------
    FACTORY_INDICATOR_V2(trade_rebalance_funds)

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
      , rsi_multiplier_(1.0)
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
          {"value", overlay_type::shared_axis},
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
          param<double>{"Sliding Gap Upper %", 1.0},                        // 5
          param<double>{"Sliding Gap Lower %", 1.0},                        // 6
          param<double>("RSI length multiplier", 1.0),                      // 7
      };
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = get<int>(params_, 1);
      mode_ = get<ohlc_modes>(params_, 2);
      portfolio_.set_fees(get<double>(params_, 3), get<double>(params_, 4));
      rsi_multiplier_ = get<double>(params_, 7);
      gradient_last_ = 0;
      gradient_this_ = 0;
      //
      first_ = true;
      portfolio_.reset(1.0, 0.0);
      //
      buffer1_ = boost::circular_buffer<float>(window_size_);
      srsi_ = stochastic_relative_strength_indicator();
      average_ = moving_average_hull(window_size_, mode_);
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

    // ----------------------------------------------------------------------------
    void rebalance(double time, double cash_fraction)
    {
      double token_total_initial = portfolio_.token_total();
      double cash_total_initial = portfolio_.cash_total();
      double sell_price_est =
          hdf5_ohlc_->get_estimated_sell_price_volume(token_total_initial, time + time_res_, 1.0);
      double buy_price_est =
          hdf5_ohlc_->get_estimated_buy_price_volume(token_total_initial, time + time_res_, 1.0);
      assert(sell_price_est <= buy_price_est);
      //
      double xrp_value_sell = (sell_price_est * token_total_initial);
      double initial_value_est = xrp_value_sell + cash_total_initial;
      //
      if (portfolio_.cash_total() < (cash_fraction * initial_value_est))
      {
        // rebalance by selling some tokens
        double excess_cash = (cash_fraction * initial_value_est) - portfolio_.cash_total();
        double excess_tokens_est = excess_cash / sell_price_est;
        GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} {} {}", "rebalance-sell", time,
            initial_value_est, portfolio_.cash_total(), excess_tokens_est);
        last_result_ = portfolio_.sell(*hdf5_ohlc_, time, excess_tokens_est);
        if (last_result_.event_price_ < sell_price_est) sell_price_est = last_result_.event_price_;
      }
      else if (portfolio_.cash_total() > cash_fraction * initial_value_est)
      {
        // rebalance by buying some tokens
        double target_cash = cash_fraction * initial_value_est;
        double excess_cash = portfolio_.cash_total() - target_cash;
        GROX_LOG_DEBUG(indicator_log, "{:>20} {} {} {} {}", "rebalance-buy", time,
            initial_value_est, portfolio_.cash_total(), excess_cash);
        last_result_ = portfolio_.buy(*hdf5_ohlc_, time, excess_cash);
        if (last_result_.event_price_ > buy_price_est) buy_price_est = last_result_.event_price_;
      }
      //
      initial_value_est = round_n((sell_price_est * token_total_initial + cash_total_initial), 6);
      double final_value_total =
          round_n((sell_price_est * portfolio_.token_total()) + portfolio_.cash_total(), 6);
      GROX_LOG_DEBUG(
          indicator_log, "{:>20} {} {}", "rebalance-value", initial_value_est, final_value_total);
      assert(final_value_total <= initial_value_est);
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
    double round_n(double value, int n)
    {
      double factor = std::pow(10.0, n);
      return std::round(value * factor) / factor;
    }

    ohlc_modes mode_;
    moving_average_hull average_;
    stochastic_relative_strength_indicator srsi_;
    boost::circular_buffer<float> buffer1_;
    portfolio portfolio_;
    //
    kernels::gradient gradient_;
    kernels::gradient rsi_gradient_;
    //
    double gradient_last_;
    double gradient_this_;
    double rsi_multiplier_;
    int window_size_;
    //
    bool first_;
    buy_sell_point last_result_;
    double time_res_;
  };

}    // namespace indicators
