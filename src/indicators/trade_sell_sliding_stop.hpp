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
    using result_type = buy_sell_point;

    // ---------------------------------------
    /// Default constructor
    trade_sell_sliding_stop(
        int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low, double gap = 3.5 / 100)
      : indicator_base("Trade: Sliding Stop", "Trade: Sliding Stop", overlays)
      , mode_(mode)
      , average_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , gap_(gap)
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
          {"Samples", candle_data{ohlc_data_resolutions::hour4, 1000}},    //
          {"Window size", 7},                                              //
          {"mode", ohlc_modes::high},                                      //
          {"Sliding Gap", 0.5 / 100}};
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(params_[1].value);
      mode_ = std::get<ohlc_modes>(params_[2].value);
      gap_ = std::get<double>(params_[3].value);
      buffer1_ = boost::circular_buffer<float>(window_size_);
      last_val_ = std::numeric_limits<double>::min();
      average_ = moving_average_exponential_volume_weighted(window_size_, mode_);
      xrp_total_ = 1;
      cash_total_ = 0;

      current_max_ = 0;
    }

    // ---------------------------------------
    result_type operator()(ohlctv_sample const& val)
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
        {    //
          last_result_ = {
              buy_sell_event_type::value, current_val_, last_result_.tokens_, last_result_.cash_};
        }
        else if (current_grad_ < 0)
        {
          // we have fallen below the trigger, and trending down - sell
          active_ = false;
          last_sell_ = current_val_;
          // compute the selling price
          double p = hdf5_ohlc_->get_estimated_sell_price(xrp_total_, val.time, 2.0);
          cash_total_ = xrp_total_ * p;
          xrp_total_ = 0;
          //
          last_result_ = {buy_sell_event_type::sell, current_val_, xrp_total_, cash_total_};
        }
        else
        {
          last_result_ = {
              buy_sell_event_type::value, current_val_, last_result_.tokens_, last_result_.cash_};
        }
      }
      else
      {
        if ((current_grad_ > 0.01))    // switched to rising trend,
        {
          active_ = true;
          current_max_ = current_val_;
          // compute the buying price
          auto p = hdf5_ohlc_->get_trade_data_by_value(cash_total_, val.time, 2.0);
          xrp_total_ = cash_total_ / p.high;
          cash_total_ = 0;
          //
          last_result_ = {buy_sell_event_type::buy, current_val_, xrp_total_, cash_total_};
        }
        else    // falling trend
        {
          last_result_ = {
              buy_sell_event_type::empty, current_val_, last_result_.tokens_, last_result_.cash_};
        }
      }
      // last_result_.tokens_ = current_grad_;
      last_val_ = current_val_;
      last_grad_ = current_grad_;
      last_sample_ = val;

      return last_result_;
    }

    // ---------------------------------------
    inline result_type getLastResult() { return last_result_; }

private:
    ohlc_modes mode_;
    moving_average_exponential_volume_weighted average_;
    boost::circular_buffer<float> buffer1_;
    double gap_;
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
