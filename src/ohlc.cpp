// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <vector>
#include <string>
//
#include "nlohmann/json.hpp"
#include "ohlc.hpp"

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
//void to_json(json& j, const xrp_amount& p) {
//    j = json{ {"currency", p.currency},
//              {"value", p.value} };
//    if (p.issuer != std::nullopt)
//    {
//        j["issuer"] = p.issuer.value();
//    }
//}

void from_json(const nlohmann::json &j, xrp_amount &p)
{
    // if this is a simple value (just plain XRP amount)
    if (j.size() == 1) {
        p.value = std::stod(j.get< std::string >());
        p.currency = currency_type::xrp;
    }
    else {
        p.value    = std::stod(j.at("value").get< std::string >());
        std::string currency = j.at("currency").get< std::string >();
        std::string issuer;
        // allow issuer OR counterparty string id
        if (j.count("counterparty") != 0)
        {
            issuer = j.at("counterparty").get< std::string >();
        }
        else if (j.count("issuer") != 0)
        {
            issuer = j.at("issuer").get< std::string >();
        }
        //
        if (currency=="XRP") {
            p.currency = currency_type::xrp;
        }
        else if (currency=="USD" && issuer=="rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B") {
            p.currency = currency_type::usd_bitstamp;
        }
        else if (currency=="USD" && issuer=="rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq") {
            p.currency = currency_type::usd_gatehub;
        }
        else {
            throw std::runtime_error("Unsupported currency");
        }
    }
}

void from_json(const nlohmann::json &j, xrpl_offer &p)
{
    p.Account   = j.at("Account").get< std::string >();
    p.TakerGets = j.at("TakerGets").get< xrp_amount >();
    p.TakerPays = j.at("TakerPays").get< xrp_amount >();
}
