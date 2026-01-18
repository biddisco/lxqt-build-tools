#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
//
#include "currency/currency.hpp"
#include "currency/ohlctv_sample.hpp"
#include "nlohmann/json.hpp"

namespace grox {
  // ----------------------------------------------------------------------------
  // bitstamp websocket ticker data
  // ----------------------------------------------------------------------------
  struct live_trade_data
  {
    double amount;
    std::string amount_str;
    std::uint64_t buy_order_id;
    std::uint64_t id;
    std::string microtimestamp;
    double price;
    std::string price_str;
    std::uint64_t sell_order_id;
    std::string timestamp;
    std::uint64_t type;
    //
    live_trade_data() = default;
  };
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(grox::live_trade_data, amount, amount_str, buy_order_id, id,
      microtimestamp, price, price_str, sell_order_id, timestamp, type);

  // ----------------------------------------------------------------------------
  // bitstamp websocket order book entry
  // ----------------------------------------------------------------------------
  struct bid_ask
  {
    std::array<double, 2> values;    // {price, amount}
    //
    bid_ask() = default;
  };
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(grox::bid_ask, values);

  // ----------------------------------------------------------------------------
  // bitstamp websocket full order book data
  // ----------------------------------------------------------------------------
  struct live_order_book
  {
    std::string timestamp;
    std::string microtimestamp;
    std::vector<bid_ask> bids;
    std::vector<bid_ask> asks;
    //
    live_order_book() = default;
  };
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_order_book, bids, asks, timestamp, microtimestamp);

  // ----------------------------------------------------------------------------
  // A type representing a currency amount in xrpl offer queries
  // ----------------------------------------------------------------------------
  struct xrp_amount
  {
    double value_;
    currency_code currency_;
  };

  // ----------------------------------------------------------------------------
  // xrpl offer type
  // ----------------------------------------------------------------------------
  // if the taker pays xrp, offer is buying xrp
  // if the takes pays usd, offer is selling xrp
  struct xrpl_offer
  {
    // used only in building bid/ask order books
    double owner_funds;
    double funded_offer;
    //
    std::string Account;
    std::string BookDirectory;
    xrp_amount TakerGets;
    xrp_amount TakerPays;

    double amount(currency_code const& c) const
    {
      // @ todo : check if currency type of C is same as type of TakerPays
      if (TakerPays.currency_.is_xrp() == c.is_xrp()) { return TakerPays.value_; }
      else { return TakerGets.value_; }
    }

    // if the taker gives xrp, offer is selling xrp
    // if the takes gives usd, offer is buying xrp
    double rate() const
    {
      if (TakerPays.currency_.is_xrp()) { return 1E6 * TakerGets.value_ / TakerPays.value_; }
      else { return 1E6 * TakerPays.value_ / TakerGets.value_; }
    }

    // we do not need to compare all fields when modifying XRP leddger offers
    // as the book directory is unique per offer node
    bool operator==(xrpl_offer const& other) const { return BookDirectory == other.BookDirectory; }

    bool operator<(xrpl_offer const& other) const
    {
      return (rate() < other.rate()) ||
          ((std::fabs(rate() - other.rate()) < 1E-6) && owner_funds > other.owner_funds);
    }

    bool operator>(xrpl_offer const& other) const
    {
      return (rate() > other.rate()) ||
          ((std::fabs(rate() - other.rate()) < 1E-6) && owner_funds > other.owner_funds);
    }

    bool unfunded(double epsilon = 0.0) const
    {
      return (funded_offer <= epsilon || TakerGets.value_ == 0) || (TakerPays.value_ == 0);
    }

    bool grox_compatible() const
    {
      bool ok = TakerGets.currency_.is_xrp() &&
          (TakerPays.currency_ == currency_code{currency_issuers::bitstamp_trust, "USD"});
      ok = ok ||
          (TakerPays.currency_.is_xrp() &&
              (TakerGets.currency_ == currency_code{currency_issuers::bitstamp_trust, "USD"}));
      return ok;
    }
  };

  // ----------------------------------------------------------------------------
  // bitstamp price history type
  // ----------------------------------------------------------------------------
  struct price
  {
    double value;
    std::int64_t time;
  };

  // ----------------------------------------------------------------------------
  std::ostream& operator<<(std::ostream& os, xrpl_offer const&);
  std::ostream& operator<<(std::ostream& os, xrp_amount const&);

  // ----------------------------------------------------------------------------
  void from_json(nlohmann::json const&, xrp_amount&);
  void from_json(nlohmann::json const&, xrpl_offer&);
  void from_json(nlohmann::json const&, price&);

}    // namespace grox
