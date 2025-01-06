#pragma once

#include <limits>
//
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_exponential_volume_weighted.hpp"

namespace indicators {

  static overlay_vector const overlays = {overlay_type::buy_sell, overlay_type::buy_sell,
      overlay_type::buy_sell, overlay_type::relative_gain};

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
        double gap = 3.5 / 100, double fee = 0.2)
      : indicator_base("Trade: Sliding Gradient", "Trade: Sliding Gradient", overlays)
      , mode_(mode)
      , average_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , gap_(gap)
      , fee_percent_(fee)
    {
      active_ = true;
    }

    // ---------------------------------------
    int num_outputs() const override { return 4; }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {                                                          //
          {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    // p-0
          {"Window size", 7},                                              // p-1
          {"mode", ohlc_modes::high},                                      // p-2
          {"Sliding Gap", 0.5 / 100},                                      // p-3
          {"Percentage fee", 0.2}};                                        // p-4
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(params_[1].value);
      mode_ = std::get<ohlc_modes>(params_[2].value);
      gap_ = std::get<double>(params_[3].value);
      fee_percent_ = std::get<double>(params_[4].value);
      buffer1_ = boost::circular_buffer<float>(window_size_);
      last_val_ = std::numeric_limits<double>::min();
      average_ = moving_average_exponential_volume_weighted(window_size_, mode_);
      xrp_total_ = 1;
      cash_total_ = 0;
      current_max_ = 0;
    }

    void buy(double time)
    {
      double fee = 0.01 * fee_percent_ * cash_total_;
      double taker_pay = cash_total_ - fee;
      //
      auto p = hdf5_ohlc_->get_trade_data_by_value(taker_pay, time, 2.0);
      xrp_total_ = taker_pay / p.high;
      cash_total_ = 0;
      //
      last_result_ = {buy_sell_event_type::buy, current_val_, (p.high * xrp_total_) + cash_total_,
          xrp_total_, cash_total_};
    }

    void sell(double time)
    {
      double fee = 0.01 * fee_percent_ * xrp_total_;
      double maker_pay = xrp_total_ - fee;
      //
      double p = hdf5_ohlc_->get_estimated_sell_price(maker_pay, time, 2.0);
      cash_total_ = maker_pay * p;
      xrp_total_ = 0;
      //
      last_result_ = {buy_sell_event_type::sell, current_val_, (p * xrp_total_) + cash_total_,
          xrp_total_, cash_total_};
    }

    // ---------------------------------------
    operator_type operator()(ohlctv_sample const& val)
    {
      // push this value into the moving average filter
      current_val_ = average_(val);

      // after reset/initialization we must reset last values
      if (last_val_ == std::numeric_limits<double>::min())
      {
        last_val_ = current_val_;
        last_sample_ = val;
        last_grad_ = 0;
        current_max_ = 0;
      }

      // update vars needed to track state
      // current_max_ = std::max(current_max_, ohlc_mode_extract(ohlc_modes::mid_high_low, val));
      current_max_ = std::max(current_max_, current_val_);
      // current gradient - currency units per day (eg 1$ per day)
      double time_elapsed = (val.time - last_sample_.time) / ohlc_data_resolutions::day;
      current_grad_ = (time_elapsed > 0) ? (current_val_ - last_val_) / time_elapsed : 0;
      //
      if (active_)
      {
        double gap = (current_max_ - current_val_);
        // if we are still within our hold range
        if ((gap_ - gap) >= 0.0)
        {
          auto p = hdf5_ohlc_->get_trade_data_by_value(last_result_.tokens_, val.time, 2.0);
          last_result_ = {buy_sell_event_type::value, current_val_,
              (p.low * xrp_total_) + cash_total_, xrp_total_, cash_total_};
        }
        else if (current_grad_ < 0)
        {
          // we have fallen below the trigger, and trending down - sell
          active_ = false;
          last_sell_ = current_val_;
          sell(val.time);
        }
        else
        {
          auto p = hdf5_ohlc_->get_trade_data_by_value(last_result_.tokens_, val.time, 2.0);
          last_result_ = {buy_sell_event_type::value, current_val_,
              (p.low * xrp_total_) + cash_total_, xrp_total_, cash_total_};
        }
      }
      else
      {
        if ((current_grad_ > 0.01))    // switched to rising trend,
        {
          active_ = true;
          current_max_ = current_val_;
          buy(val.time);
        }
        else    // falling trend
        {
          auto p = hdf5_ohlc_->get_trade_data_by_value(last_result_.tokens_, val.time, 2.0);
          last_result_ = {buy_sell_event_type::empty, current_val_,
              (p.low * xrp_total_) + cash_total_, xrp_total_, cash_total_};
        }
      }
      // last_result_.tokens_ = current_grad_;
      last_val_ = current_val_;
      last_grad_ = current_grad_;
      last_sample_ = val;

      return last_result_;
    }

    // ---------------------------------------
    inline operator_type getLastResult() { return last_result_; }

private:
    ohlc_modes mode_;
    moving_average_exponential_volume_weighted average_;
    boost::circular_buffer<float> buffer1_;
    double gap_;
    double fee_percent_;
    int window_size_;
    double current_val_;
    double current_max_;
    double current_grad_;
    double last_val_;
    double last_grad_;
    buy_sell_point last_result_;
    bool active_;
    double last_sell_;
    double xrp_total_;
    double cash_total_;
    ohlctv_sample last_sample_;
  };

}    // namespace indicators
