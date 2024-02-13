#pragma once
//
#include "currency/ohlctv_sample.hpp"
//
#include <optional>
#include <string>
#include <vector>
//
#include "currency/currency.hpp"
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
    double value;
    std::optional<currency_code> currency = std::nullopt;
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

    // if there is no issuer, then it must be native xrp currency
    bool is_xrp(xrp_amount const x) const
    {
      return !x.currency.has_value();
    }

    double amount(currency const& c) const
    {
      // @ todo : check if currency type of C is same as type of TakerPAys
      if (is_xrp(TakerPays) == c.is_xrp())
      {
        return TakerPays.value;
      }
      else
      {
        return TakerGets.value;
      }
    }

    // if the taker gives xrp, offer is selling xrp
    // if the takes gives usd, offer is buying xrp
    double rate() const
    {
      if (is_xrp(TakerPays))
      {
        return 1E6 * TakerGets.value / TakerPays.value;
      }
      else
      {
        return 1E6 * TakerPays.value / TakerGets.value;
      }
    }

    // we do not need to compare all fields when modifying XRP leddger offers
    // as the book directory is unique per offer node
    bool operator==(xrpl_offer const& other) const
    {
      return BookDirectory == other.BookDirectory;
    }

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
      return (funded_offer <= epsilon || TakerGets.value == 0) || (TakerPays.value == 0);
    }

    bool grox_compatible() const
    {
      return ((is_xrp(TakerGets) &&
                (TakerPays.currency == currency_code{currency::bitstamp_trust, "USD"})) ||
        (is_xrp(TakerPays) &&
          (TakerGets.currency == currency_code{currency::bitstamp_trust, "USD"})));
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
  void to_json(nlohmann::json&, xrp_amount const&);
  void from_json(nlohmann::json const&, xrp_amount&);
  void from_json(nlohmann::json const&, xrpl_offer&);
  void from_json(nlohmann::json const&, price&);

}    // namespace grox
