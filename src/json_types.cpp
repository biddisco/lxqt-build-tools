// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <iostream>
#include <string>
#include <vector>
//
#include "json_types.hpp"
#include "nlohmann/json.hpp"

std::ostream& operator<<(std::ostream& os, QwtOHLCSample const& x)
{
#if 0
    os << "Time: "   << x.time << " "
       << "Open: "   << x.open << " "
       << "High: "   << x.high << " "
       << "Low: "    << x.low << " "
       << "Close: "  << x.close << " "
       << "Volume: " << x.volume;
#else
  os << "T: " << x.time << " "
     << "O: " << x.open << " "
     << "H: " << x.high << " "
     << "L: " << x.low << " "
     << "C: " << x.close << " "
     << "V: " << x.volume;
#endif
  return os;
}

std::ostream& operator<<(std::ostream& os, xrp_amount const& x)
{
  os << "Value: " << x.value << " "
     << "Currency: ";
  if (x.currency == currency_type::xrp)
    os << "xrp";
  else if (x.currency == currency_type::usd_bitstamp)
    os << "usd_bitstamp";
  else if (x.currency == currency_type::eur_bitstamp)
    os << "eur_bitstamp";
  else if (x.currency == currency_type::usd_gatehub)
    os << "usd_gatehub";
  else
    os << "other";
  return os;
}

std::ostream& operator<<(std::ostream& os, xrpl_offer const& x)
{
  os << "Account: " << x.Account << " "
     << "BookDirectory: " << x.BookDirectory << " "
     << "TakerPays: " << x.TakerPays << " "
     << "TakerGets: " << x.TakerGets << " "
     << "Rate: " << x.rate() << " "
     << "Funds: " << x.owner_funds;
  return os;
}

// ----------------------------------------------------------------------------
// ohlc data
// ----------------------------------------------------------------------------
using nlohmann::json;

//Q_DECLARE_METATYPE(ohlc_string)
//Q_DECLARE_METATYPE(std::vector<ohlc_string>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ohlc_string,
//    close, high, low, open, timestamp, volume);

//Q_DECLARE_METATYPE(ohlc)
//Q_DECLARE_METATYPE(std::vector<ohlc>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ohlc,
//    close, high, low, open, timestamp, volume);

// ----------------------------------------------------------------------------
// bitstamp websocket ticker data
// ----------------------------------------------------------------------------
//Q_DECLARE_METATYPE(live_trades)
//Q_DECLARE_METATYPE(std::vector<live_trades>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_trades,
//    amount, amount_str, buy_order_id, id, microtimestamp, price_str, sell_order_id, timestamp, type);

// ----------------------------------------------------------------------------
// bitstamp websocket order book
// ----------------------------------------------------------------------------
//Q_DECLARE_METATYPE(bid_ask)
//Q_DECLARE_METATYPE(std::vector<bid_ask>*)
////NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, price, amount);
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, values);

//Q_DECLARE_METATYPE(live_order_book)
//Q_DECLARE_METATYPE(std::vector<live_order_book>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_order_book, bids, asks, timestamp, microtimestamp);

// Due to std::optional, we must provide serialization ourselves
//void to_json(json& j, xrp_amount const& p) {
//    j = json{ {"currency", p.currency},
//              {"value", p.value} };
//    if (p.issuer != std::nullopt)
//    {
//        j["issuer"] = p.issuer.value();
//    }
//}

/* New style using RPC json API
    "account": "rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY",
    "balance": "63.8354397",
    "currency": "5041534100000000000000000000000000000000",
    "limit": "100000000",
    "limit_peer": "0",
    "no_ripple": true,
    "no_ripple_peer": false,
    "quality_in": 0,
    "quality_out": 0
*/
/* Old style using v2 data API
    "currency": "5041534100000000000000000000000000000000",
    "counterparty": "rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY",
    "value": "63.8354397"
*/

void from_json(nlohmann::json const& j, xrp_amount& p)
{
  // if this is a simple value (just plain XRP amount)
  if (j.size() == 1)
  {
    p.value = std::stod(j.get<std::string>());
    p.currency = currency_type::xrp;
  }
  else
  {
    // allow balance OR value string id
    if (j.count("balance") != 0)
    {
      p.value = std::stod(j.at("balance").get<std::string>());
    }
    else if (j.count("value") != 0)
    {
      p.value = std::stod(j.at("value").get<std::string>());
    }
    else
      throw std::runtime_error("No value in currency amount");
    //
    std::string currency = j.at("currency").get<std::string>();
    //
    std::string issuer;
    // allow account/issuer/counterparty string id
    if (j.count("account") != 0)
    {
      issuer = j.at("account").get<std::string>();
    }
    else if (j.count("counterparty") != 0)
    {
      issuer = j.at("counterparty").get<std::string>();
    }
    else if (j.count("issuer") != 0)
    {
      issuer = j.at("issuer").get<std::string>();
    }
    //
    p.trustline = {issuer, currency};
    p.currency = get_currency_type({issuer, currency});
  }
}

void from_json(nlohmann::json const& j, xrpl_offer& p)
{
  bool ok = true;
  p.funded_offer = -1;
  p.Account = j.at("Account").get<std::string>();
  p.BookDirectory = j.at("BookDirectory").get<std::string>();
  p.TakerGets = j.at("TakerGets").get<xrp_amount>();
  p.TakerPays = j.at("TakerPays").get<xrp_amount>();

  // we track unfunded offers using owner funds - this code is obsolete
#ifdef OLD_TAKER_PAYS_FUNDED
  if (j.contains("taker_gets_funded"))
  {
    xrpl_offer temp = p;
    temp.TakerGets = j.at("taker_gets_funded").get<xrp_amount>();
    temp.TakerPays = j.at("taker_pays_funded").get<xrp_amount>();
    if (temp.TakerGets.value > 0 && temp.TakerPays.value > 0)
    {
      p = temp;
    }
  }
#endif
  // The offer might be for more than the account actually holds
  // so we must track actual funds when building order book later
  if (j.contains("owner_funds"))
  {
    p.owner_funds = std::stod(j.at("owner_funds").get<std::string>());
  }
  else
  {
    p.owner_funds = -1;
  }
}
