#pragma once

#include <limits>
//
#include <range/v3/view.hpp>
#include <boost/circular_buffer.hpp>
//
#include "data/ohlc_data_resolutions.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"
#include "indicators/moving_average_volume_weighted.hpp"

namespace indicators {

  //----------------------------------------------------------------------------
  class sliding_stop : public indicator_base
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
    sliding_stop(
        int window_size = 7, ohlc_modes mode = ohlc_modes::mid_high_low, double gap = 3.5 / 100)
      : indicator_base("Sliding Stop", "Trade: Sliding Stop", overlay_type::buy_sell)
      , mode_(mode)
      , average_{}
      , buffer1_(window_size)
      , window_size_(window_size)
      , gap_(gap)
    {
      active_ = true;
    }

    // ---------------------------------------
    int num_outputs() const override { return 3; }

    // ---------------------------------------
    /// fields required for auto gui generation
    void init_params() override
    {
      params_ = {//
          std::make_tuple<QString, param_types>(
              "Samples", candle_data{ohlc_data_resolutions::minute15, 5000}),
          std::make_tuple<QString, param_types>("Window size", 7),
          std::make_tuple<QString, param_types>("mode", ohlc_modes::mid_open_close),
          std::make_tuple<QString, param_types>("Sliding Gap", 3.5 / 100)};
    }

    // ---------------------------------------
    /// initialize internals from a parameter list
    void initialize() override
    {
      window_size_ = std::get<int>(std::get<1>(params_[1]));
      mode_ = std::get<ohlc_modes>(std::get<1>(params_[2]));
      gap_ = std::get<double>(std::get<1>(params_[3]));
      buffer1_ = boost::circular_buffer<float>(window_size_);
      last_val_ = std::numeric_limits<double>::min();
      average_ = moving_average_volume_weighted(window_size_, mode_);

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
        last_grad_ = 0;
        current_max_ = 0;
      }

      // update vars needed to track state
      current_max_ = std::max(current_max_, current_val_);
      current_grad_ = (last_val_ - current_val_);
      //
      if (active_)
      {
        double gap = (current_max_ - current_val_);
        // if we are still within our hold range
        if ((gap_ - gap) >= 0.0)
        {    //
          last_result_ = {buy_sell_event_type::value, current_val_};
        }
        else
        {
          // we have fallen below the trigger, sell
          active_ = false;
          last_result_ = {buy_sell_event_type::sell, current_val_};
        }
      }
      else
      {
        if ((current_grad_ < 0) && (last_grad_ >= 0))    // switched to rising trend,
        {
          active_ = true;
          current_max_ = current_val_;
          last_result_ = {buy_sell_event_type::buy, current_val_};
        }
        else    // falling trend
        {
          last_result_ = {buy_sell_event_type::empty, current_val_};
        }
      }

      last_val_ = current_val_;
      last_grad_ = current_grad_;
      return last_result_;
    }

    // ---------------------------------------
    inline result_type getLastResult() { return last_result_; }

private:
    ohlc_modes mode_;
    moving_average_volume_weighted average_;
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
  };

}    // namespace indicators
