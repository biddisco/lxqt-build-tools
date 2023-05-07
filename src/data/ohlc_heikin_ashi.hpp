#pragma once

// STL
#include <iostream>
#include <optional>
// Qwt
#include <QwtOHLCSample>

// ----------------------------------------------------------------------------
enum buy_sell_type
{
  no_event = 0,
  buy_event = 1,
  sell_event = 2,
};

// ----------------------------------------------------------------------------
struct trade_event
{
  double time_;
  buy_sell_type type_;
};

// ----------------------------------------------------------------------------
struct ohlc_heikin_ashi
{
  bool first_;
  QwtOHLCSample prev_;

  ohlc_heikin_ashi()
    : first_(true)
    , prev_()
  {
  }

  ohlc_heikin_ashi(const QwtOHLCSample& ohlc)
    : first_(false)
    , prev_(ohlc)
  {
    // if inital ohlc sample is empty, set flag
    if (prev_.time == 0)
    {
      first_ = true;
    }
  }

  std::optional<QwtOHLCSample> operator()(std::optional<QwtOHLCSample> ohlc_o)
  {
    // exit or get the value
    if (!ohlc_o.has_value())
      return std::nullopt;
    const QwtOHLCSample& ohlc = ohlc_o.value();

    // first point in plot needs a prev open/close
    if (first_)
    {
      prev_ = ohlc;
      first_ = false;
    }
    //
    double close = 0.25 * (ohlc.open + ohlc.high + ohlc.low + ohlc.close);
    double open = 0.50 * (prev_.open + prev_.close);
    double high = std::max(std::max(ohlc.open, ohlc.close), ohlc.high);
    double low = std::min(std::min(ohlc.open, ohlc.close), ohlc.low);
    QwtOHLCSample result(ohlc.time, open, high, low, close, ohlc.volume);
    prev_ = result;
    return result;
  }
};

// ----------------------------------------------------------------------------
struct heikin_ashi_transition
{
  bool first_;
  bool prev_;

  heikin_ashi_transition()
    : first_(true)
    , prev_(false)
  {
  }

  buy_sell_type operator()(std::optional<QwtOHLCSample> ha_o)
  {
    // exit or get the value
    if (!ha_o.has_value())
      return buy_sell_type::no_event;
    const QwtOHLCSample& ha = ha_o.value();

    // first point in plot needs a prev open/close
    if (first_)
    {
      prev_ = (ha.open < ha.close);
      first_ = false;
    }
    else
    {
      bool green = (ha.open < ha.close);
      if (green != prev_)
      {
        prev_ = green;
        return (green ? buy_sell_type::buy_event : buy_sell_type::sell_event);
      }
    }
    return buy_sell_type::no_event;
  }
};

// ----------------------------------------------------------------------------
struct add_time_filter
{
  add_time_filter() {}

  trade_event operator()(buy_sell_type bs, double time)
  {
    return {time, bs};
  }
};
