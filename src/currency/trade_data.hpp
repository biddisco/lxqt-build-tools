#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <memory>
//
#include "currency/currency.hpp"

class exchange;

enum trade_type
{
  buy = 0,
  sell,
  trade,
};

enum order_type
{
  limit = 0,
  market,
  fill_or_kill,
};

// trade type : buy = 0, sell = 1
struct trade_data
{
  std::shared_ptr<exchange> network_;
  std::string wallet_;
  issued_currency taker_payc_;
  issued_currency taker_getc_;
  double taker_pay_;
  double taker_get_;
  double exchange_rate_;
  double fee_percent_;
  double fee_fixed_;
  std::uint64_t id_;
  std::string datetime_;
  bool confirmed_;

  // if we are buying or selling xrp, then how many?
  double get_xrp_amount() const
  {
    // if taker gets xrp, we must be selling xrp
    auto tradetype = get_trade_type();
    if (tradetype == trade_type::buy)
    {
      return taker_pay_;
    }
    else if (tradetype == trade_type::sell)
    {
      return taker_get_;
    }
    else
    {
      throw std::runtime_error("Not an xrp transaction");
    }
    return 0;
  }

  double get_price() const
  {
    // if taker gets xrp, we must be selling xrp
    auto tradetype = get_trade_type();
    if (tradetype == trade_type::buy)
    {
      return taker_get_ / taker_pay_;
    }
    else if (tradetype == trade_type::sell)
    {
      return taker_pay_ / taker_get_;
    }
    else
    {
      throw std::runtime_error("Not an xrp transaction");
    }
    return 0;
  }

  trade_type get_trade_type() const
  {
    // if taker pays us xrp, we are buying xrp
    // if takets gets xrp from us, we are selling it
    if (taker_payc_.is_xrp())
      return trade_type::buy;
    if (taker_getc_.is_xrp())
      return trade_type::sell;
    return trade_type::trade;
  }
};
