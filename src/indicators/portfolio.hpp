#pragma once

#include <cassert>
#include <cstdint>
//
#include "data/ohlc_dataset_view.hpp"
#include "debug/logging.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
/// A paper-trading portfolio used by strategy indicators. Holds cash and token
/// balances, applies buy/sell fees, and computes portfolio value against an
/// ohlc_dataset_view (the price provider).
///
/// Extracted from the duplicated accounting in trade_sell_sliding_stop and
/// trade_rebalance_funds. Both strategies will compose a portfolio instance
/// instead of inlining their own xrp_total_/cash_total_ logic.
namespace indicators {

  static auto pfolio_log = grox::log::create("Portfolio");

  class portfolio
  {
public:
    portfolio()
      : cash_total_(0)
      , token_total_(0)
      , fee_percent_buy_(0)
      , fee_percent_sell_(0)
      , time_res_(0)
    {
    }

    // ----------------------------------------------------------------------------
    /// Set the fee percentages (0.2 = 0.2%)
    void set_fees(double fee_buy, double fee_sell)
    {
      fee_percent_buy_ = fee_buy;
      fee_percent_sell_ = fee_sell;
    }

    // ----------------------------------------------------------------------------
    /// Set the time resolution offset used when querying the price provider
    void set_time_resolution(double res) { time_res_ = res; }

    // ----------------------------------------------------------------------------
    /// Reset to initial state with the given starting balances
    void reset(double token_start = 1.0, double cash_start = 0.0)
    {
      token_total_ = token_start;
      cash_total_ = cash_start;
    }

    // ----------------------------------------------------------------------------
    double token_total() const { return token_total_; }
    double cash_total() const { return cash_total_; }

    // ----------------------------------------------------------------------------
    /// Current portfolio value at the given price
    double value(double price) const { return (price * token_total_) + cash_total_; }

    // ----------------------------------------------------------------------------
    /// Buy tokens with the given cash amount at the estimated price from the
    /// view. Returns a buy_sell_point describing the trade.
    buy_sell_point buy(ohlc_dataset_view const& view, double time, double cash_amount)
    {
      double p = view.get_estimated_buy_price_value(cash_amount, time + time_res_, 2.0);
      double initial_value = (p * token_total_) + cash_total_;
      //
      double fee = 0.01 * fee_percent_buy_ * cash_amount;
      double taker_pay = cash_amount - fee;
      double tokens_bought = taker_pay / p;
      //
      token_total_ += tokens_bought;
      cash_total_ -= cash_amount;
      //
      GROX_LOG_DEBUG(pfolio_log,
          "{:>20} bought {:.2f} tokens at price {:.2f} for total {:.2f} "
          "with fee {:.2f}",
          "buy", tokens_bought, p, taker_pay, fee);
      GROX_LOG_DEBUG(pfolio_log,
          "{:>20} initial_value {:.2f} final_value {:.2f} "
          "cash_total {:.2f} token_total {:.2f}",
          "buy", initial_value, (p * token_total_) + cash_total_, cash_total_, token_total_);
      //
      buy_sell_point result = {.event_type_ = buy_sell_event_type::buy,
          .event_time_ = time + time_res_,
          .price_ = p,
          .event_price_ = p,
          .value_ = (p * token_total_) + cash_total_,
          .tokens_ = token_total_,
          .cash_ = cash_total_};
      //
      assert(token_total_ >= 0);
      assert(cash_total_ >= 0);
      assert(initial_value >= result.value_);
      return result;
    }

    // ----------------------------------------------------------------------------
    /// Sell the given token amount at the estimated price from the view.
    /// Returns a buy_sell_point describing the trade.
    buy_sell_point sell(ohlc_dataset_view const& view, double time, double token_amount)
    {
      double p = view.get_estimated_sell_price_volume(token_amount, time + time_res_, 2.0);
      double initial_value = (p * token_total_) + cash_total_;
      //
      double taker_pay = token_amount * p;
      double fee = 0.01 * fee_percent_sell_ * taker_pay;
      cash_total_ += taker_pay - fee;
      token_total_ -= token_amount;
      //
      GROX_LOG_DEBUG(pfolio_log,
          "{:>20} sold {:.2f} tokens at price {:.2f} for total {:.2f} "
          "with fee {:.2f}",
          "sell", token_amount, p, taker_pay, fee);
      GROX_LOG_DEBUG(pfolio_log,
          "{:>20} initial_value {:.2f} final_value {:.2f} "
          "cash_total {:.2f} token_total {:.2f}",
          "sell", initial_value, (p * token_total_) + cash_total_, cash_total_, token_total_);
      //
      buy_sell_point result = {.event_type_ = buy_sell_event_type::sell,
          .event_time_ = time + time_res_,
          .price_ = p,
          .event_price_ = p,
          .value_ = (p * token_total_) + cash_total_,
          .tokens_ = token_total_,
          .cash_ = cash_total_};
      //
      assert(token_total_ >= 0);
      assert(cash_total_ >= 0);
      assert(initial_value >= result.value_);
      return result;
    }

private:
    double cash_total_;
    double token_total_;
    double fee_percent_buy_;
    double fee_percent_sell_;
    double time_res_;
  };

}    // namespace indicators
