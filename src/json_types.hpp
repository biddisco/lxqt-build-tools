#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <QwtOHLCSample>
//
#include <optional>
#include <string>
#include <vector>
//
#include "currency.hpp"
#include "nlohmann/json.hpp"

// ----------------------------------------------------------------------------
// ohlc data
// ----------------------------------------------------------------------------
using nlohmann::json;

struct ohlc : QwtOHLCSample
{
  //
  ohlc() = default;
  ohlc(const QwtOHLCSample& other)
    : QwtOHLCSample(other.time, other.open, other.high, other.low, other.close, other.volume)
  {
  }
};

Q_DECLARE_METATYPE(ohlc)
Q_DECLARE_METATYPE(std::vector<ohlc>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ohlc, time, open, high, low, close, volume);

std::ostream& operator<<(std::ostream& os, const QwtOHLCSample&);

// ----------------------------------------------------------------------------
// bitstamp websocket ticker data
// ----------------------------------------------------------------------------
struct live_trades
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
  live_trades() = default;
};

Q_DECLARE_METATYPE(live_trades)
Q_DECLARE_METATYPE(std::vector<live_trades>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_trades, amount, amount_str, buy_order_id, id,
  microtimestamp, price, price_str, sell_order_id, timestamp, type);

// ----------------------------------------------------------------------------
// bitstamp websocket order book
// ----------------------------------------------------------------------------
struct numeric_val
{
  double value;
};

typedef std::vector<numeric_val> numeric_vector;

struct bid_ask
{
  std::array<double, 2> values;
  //    double price;
  //    double amount;
  //
  bid_ask() = default;
};

Q_DECLARE_METATYPE(bid_ask)
Q_DECLARE_METATYPE(std::vector<bid_ask>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, price, amount);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, values);

struct live_order_book
{
  std::string timestamp;
  std::string microtimestamp;
  std::vector<bid_ask> bids;
  std::vector<bid_ask> asks;
  //
  live_order_book() = default;
};

Q_DECLARE_METATYPE(live_order_book)
Q_DECLARE_METATYPE(std::vector<live_order_book>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_order_book, bids, asks, timestamp, microtimestamp);

struct xrp_amount
{
  double value;
  currency_type currency;
  std::optional<issued_currency> trustline = std::nullopt;
};

Q_DECLARE_METATYPE(xrp_amount)
Q_DECLARE_METATYPE(std::vector<xrp_amount>*)

// if the taker pays xrp, offer is buying xrp
// if the takes pays usd, offer is selling xrp
struct xrpl_offer
{
  public:
  // used only in building bid/ask order books
  double owner_funds;
  double funded_offer;

  public:
  std::string Account;
  std::string BookDirectory;
  xrp_amount TakerGets;
  xrp_amount TakerPays;

  double amount(currency_type c) const
  {
    if (TakerPays.currency == c)
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
    if (TakerPays.currency == currency_type::xrp)
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
  bool operator==(const xrpl_offer& other) const
  {
    return BookDirectory == other.BookDirectory;
  }

  bool operator<(const xrpl_offer& other) const
  {
    return (rate() < other.rate()) ||
      ((std::fabs(rate() - other.rate()) < 1E-6) && owner_funds > other.owner_funds);
  }

  bool operator>(const xrpl_offer& other) const
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
    return ((TakerGets.currency == currency_type::xrp &&
              TakerPays.currency == currency_type::usd_bitstamp) ||
      (TakerPays.currency == currency_type::xrp &&
        TakerGets.currency == currency_type::usd_bitstamp));
  }
};

std::ostream& operator<<(std::ostream& os, const xrpl_offer&);
std::ostream& operator<<(std::ostream& os, const xrp_amount&);

void to_json(json& j, const xrp_amount& p);
void from_json(const nlohmann::json& j, xrp_amount& p);
void from_json(const nlohmann::json& j, xrpl_offer& p);
