#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <vector>
#include <string>
//
#include "nlohmann/json.hpp"

// ----------------------------------------------------------------------------
// ohlc data
// ----------------------------------------------------------------------------
using nlohmann::json;

struct ohlc_string {
    std::string  close;
    std::string  high;
    std::string  low;
    std::string  open;
    std::string  timestamp;
    std::string  volume;
    //
    ohlc_string() = default;
};

Q_DECLARE_METATYPE(ohlc_string)
Q_DECLARE_METATYPE(std::vector<ohlc_string>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ohlc_string,
    close, high, low, open, timestamp, volume);

struct ohlc {
    double   close;
    double   high;
    double   low;
    double   open;
    double   timestamp;
    double   volume;
    //
    ohlc() = default;
    ohlc(const ohlc_string &s) {
        close       = std::atof(s.close.c_str());
        high        = std::atof(s.high.c_str());
        low         = std::atof(s.low.c_str());
        open        = std::atof(s.open.c_str());
        volume      = std::atof(s.volume.c_str());
        timestamp   = std::atof(s.timestamp.c_str());
    }
};

Q_DECLARE_METATYPE(ohlc)
Q_DECLARE_METATYPE(std::vector<ohlc>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ohlc,
    close, high, low, open, timestamp, volume);


// ----------------------------------------------------------------------------
// bitstamp websocket ticker data
// ----------------------------------------------------------------------------
struct live_trades {
    double          amount;
    std::string     amount_str;
    std::uint64_t   buy_order_id;
    std::uint64_t   id;
    std::string     microtimestamp;
    double          price;
    std::string     price_str;
    std::uint64_t   sell_order_id;
    std::string     timestamp;
    std::uint64_t   type;
    //
    live_trades() = default;
};

Q_DECLARE_METATYPE(live_trades)
Q_DECLARE_METATYPE(std::vector<live_trades>*)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(live_trades,
    amount, amount_str, buy_order_id, id, microtimestamp, price_str, sell_order_id, timestamp, type);

// ----------------------------------------------------------------------------
// bitstamp websocket order book
// ----------------------------------------------------------------------------
struct numeric_val {
    double value;
};

typedef std::vector<numeric_val> numeric_vector;

struct bid_ask {
    std::array<double,2> values;
//    double price;
//    double amount;
    //
    bid_ask() = default;
};

Q_DECLARE_METATYPE(bid_ask)
Q_DECLARE_METATYPE(std::vector<bid_ask>*)
//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, price, amount);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(bid_ask, values);

struct live_order_book {
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

struct xrp_balances {
    std::string currency;
    std::string value;
    std::optional<std::string> counterparty = std::nullopt;
};

// Due to std::optional, we must provide serialization ourselves
void to_json(json& j, const xrp_balances& p) {
    j = json{ {"currency", p.currency},
              {"value", p.value} };
    if (p.counterparty != std::nullopt)
    {
        j["counterparty"] = p.counterparty.value();
    }
}

void from_json(const nlohmann::json &j, xrp_balances &p)
{
    p.currency = j.at("currency").get< std::string >();
    p.value    = j.at("value").get< std::string >();
    if (j.count("counterparty") != 0)
    {
        p.counterparty = j.at("counterparty").get< std::string >();
    }
}

Q_DECLARE_METATYPE(xrp_balances)
Q_DECLARE_METATYPE(std::vector<xrp_balances>*)
