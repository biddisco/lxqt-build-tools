// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <qwt_scale_div.h>
//
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
//
#include <boost/format.hpp>
#include <boost/iterator/zip_iterator.hpp>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
//
#include "nlohmann/json.hpp"
#include "ohlc.hpp"
//
#include "plots/OrderBookCurve.h"
#include "plots/OrderBookPlot.h"
//
#include "order_book.hpp"

bool startswith(const std::string_view str, const std::string& sub)
{
    // rev search - pos=0, limits search to pos or earlier
    // equivalent to if data.startswith(...)
    if (str.rfind(sub, 0) != 0)
    {
        return false;
    }
    return true;
}

#ifndef DEBUG_ONLY
#define DEBUG_ONLY(x)
#endif

#define DEBUG_ALWAYS(x) { \
    std::stringstream temp; temp << x; \
    std::cout << temp.str() << std::endl; }

//#define GROX_ARBITRAGE_TEST_MODE 1

bool startswith(const std::string_view str, const std::string& sub);

// ----------------------------------------------------------------------------
// Base order book class provides access to top bids/asks
// plotting and other representations of the orders
// ----------------------------------------------------------------------------
order_book_base::order_book_base(std::shared_ptr<OrderBookPlot> obp, bool secondaxis)
{
#ifdef GROX_ARBITRAGE_TEST_MODE
    std::cerr << "**********************************************\n"
              << "Warning TEST_MODE enabled, price data invalid\n"
              << "**********************************************\n";
#endif
    OrderBookPlot_ = obp;
    //
    prev_xmin = 0;
    prev_xmax = 0;
    prev_ymax = 0;
    //
    bid_curve_ = new OrderBookCurve();
    ask_curve_ = new OrderBookCurve();
    //
    if (secondaxis)
    {
        bid_curve_->setSegmentInfo(0, 100, Qt::darkYellow, 3);
        ask_curve_->setSegmentInfo(0, 100, Qt::darkMagenta, 3);
        bid_curve_->setYAxis(QwtPlot::yRight);
        ask_curve_->setYAxis(QwtPlot::yRight);
    }
    else
    {
        bid_curve_->setSegmentInfo(0, 100, Qt::green, 3);
        ask_curve_->setSegmentInfo(0, 100, Qt::red, 3);
        bid_curve_->setYAxis(QwtPlot::yLeft);
        ask_curve_->setYAxis(QwtPlot::yLeft);
    }
    bid_curve_->attach(obp.get());
    ask_curve_->attach(obp.get());
}

order_book_base::~order_book_base()
{
    // release shared_ptr reference early
    OrderBookPlot_ = nullptr;
    // owned by plot?
    //delete bid_curve_;
    //delete ask_curve_;
}

void order_book_base::update_graph_limits()
{
    // pick x min max limits so they don't jump around constantly
    double xrange = asks.rate.back() - bids.rate.back();
    double xscale = 0.25 * std::pow(10, static_cast<int>(std::log10(xrange)));
    double xmin = std::floor(bids.rate.back() / xscale) * xscale;
    double xmax = std::ceil(asks.rate.back() / xscale) * xscale;
    //
    // pick y min max limits so they don't jump around constantly
    double yrange = std::max(bids.total.back(), asks.total.back());
    double yscale = 0.25 * std::pow(10, static_cast<int64_t>(std::log10(yrange)));
    double ymin = 0.0;
    double ymax = (std::ceil(yrange / yscale)) * yscale;
    //
    static bool first_time = true;
    if (first_time)
    {
        prev_xmin = xmin;
        prev_xmax = xmax;
        prev_ymax = ymax;
        first_time = false;
    }
    else
    {
        double x1 = (xmin - prev_xmin) / 50.0;
        double x2 = (prev_xmax - xmax) / 50.0;
        double y2 = (prev_ymax - ymax) / 50.0;
        prev_xmin += x1;
        prev_xmax -= x2;
        prev_ymax -= y2;
    }
    //
    OrderBookPlot_->setAxisScale(QwtPlot::xBottom, prev_xmin, prev_xmax);
    OrderBookPlot_->setAxisScale(QwtPlot::yLeft, ymin, prev_ymax);
    OrderBookPlot_->setAxisScale(QwtPlot::yRight, ymin, prev_ymax / 5.0);
}

// produces a simple string representation of the order book
// from the bid/ask lists
std::string order_book_base::order_book_string()
{
    // string header line
    std::stringstream temp;
    if (bids.orig.size()==0) {
        // title format string
        boost::format title("%8s %10s %10s | %10s %10s %8s\n");
        temp << title % "Total" % "Size" % "Bid" % "Ask" % "Size" % "Total";
        // numeric entries format string
        boost::format num("%8.0f %10.2f %10.4f | %10.4f %10.2f %8.0f\n");
        // iterate over bids/asks
        auto zipped = ranges::views::zip(
            bids.total, bids.size, bids.rate, asks.rate, asks.size, asks.total);
        for (auto const& z : zipped)
        {
            temp << num % std::get<0>(z) % std::get<1>(z) % std::get<2>(z) % std::get<3>(z) %
                    std::get<4>(z) % std::get<5>(z);
        }
    }
    else {
        // title format string
        boost::format title("%8s %10s %10s %10s | %10s %10s %10s %8s\n");
        temp << title % "Total" % "Size" % "Orig" % "Bid" % "Ask" % "Size" % "Orig" % "Total";
        // numeric entries format string
        boost::format num("%8.0f %10.2f %10.2f %10.4f | %10.4f %10.2f %10.2f %8.0f\n");
        // iterate over bids/asks
        auto zipped = ranges::views::zip(
            bids.total, bids.size, bids.orig, bids.rate, asks.rate, asks.size, asks.orig, asks.total);
        for (auto const& z : zipped)
        {
            temp << num % std::get<0>(z) % std::get<1>(z) % std::get<2>(z) % std::get<3>(z) %
                    std::get<4>(z) % std::get<5>(z) % std::get<6>(z) % std::get<7>(z);
        }
    }
    //
    return temp.str();
}

// given a max amount to spend, how much of this ask to take
std::pair<double, double> order_book_base::buy_nibble(double max_spend,
    double fee_percent, double fee_fixed, double size, double rate) const
{
    double fee_mx = fee_percent / 100.0;
    if (max_spend > 0)
    {
        double spend = (max_spend * (1.0 - fee_mx)) - fee_fixed;
        double tokens = std::min(spend / rate, size);
        double spent = tokens * rate * (1.0 + fee_mx);
        return std::make_pair(tokens, spent);
    }
    // this is a request for purchase cost for a fixed amount of tokens
    else
    {
        double spent = size * rate * (1.0 + fee_mx);
        return std::make_pair(size, spent);
    }
}

// given some tokens to sell, how much of this bid to take
std::pair<double, double> order_book_base::sell_nibble(double max_tokens,
    double fee_percent, double fee_fixed, double size, double rate) const
{
    double fee_mx = fee_percent / 100.0;
    double t_recv = std::min(size, max_tokens);
    double m_recv = t_recv * rate;
    double a_recv = (m_recv * (1.0 - fee_mx)) - fee_fixed;
    return std::make_pair(t_recv, a_recv);
}

//
order_book_base::arb_vector order_book_base::compute_arbitrage(
    double budget, double fee_pc, double fee_fix, order_book_base const& other) const
{
    if (asks.size.size()==0 || other.bids.size.size()==0) return {};
    //
    order_book_base::arb_vector buy_here_sell_there;
    order_book_base::arb_vector sell_here_buy_there;
    //
    auto here_ask_zipped = ranges::views::zip(asks.size, asks.rate);
    auto there_bid_zipped = ranges::views::zip(other.bids.total, other.bids.rate);
    auto sell_point = there_bid_zipped.begin();
    //
    double sell_size, sell_rate;
    std::tie(sell_size, sell_rate) = *sell_point;
    //
    // title format string
    std::stringstream temp;
    boost::format title("%10s %10s %10s %10s | %10s %10s %10s %10s | %10s %10s %10s %10s\n");
    temp << title % "Buy" % "Avail" % "Price" % "Cost" %
            "Sell" % "Avail" % "Price" % "Receive" %
            "Gain" % "%" % "C_Gain" % "C_%";
    // numeric entries format string
    boost::format num(
        "%11.4f %11.4f %10.4f %10.4f | %11.4f %11.4f %10.4f %10.4f | %10.4f %10.4f %10.4f %10.4f\n");
    boost::format num_partial(
        "%11s %11s %10s %10s | %11.4f %11.4f %10.4f %10.4f | %10.4f %10.4f %10.4f %10.4f\n");
    boost::format num_summary(
        "%11s %11s %10s %10.4f | %11.4f %11s %10s %10.4f | %10.4f %10.4f %10.4f %10.4f\n");
    //
    using trade_set = std::tuple<double, double, double, double, double, double, double,
        double, double, double>;
    std::vector<trade_set> trades;
    //
    double spend_budget = budget;
    double cum_gain = 0;
    double cum_pc = 0;
    for (auto const& o : here_ask_zipped)
    {
        // if we buy the sells present in the ask list, how much do we pay?
        double ask_size, ask_rate;
        std::tie(ask_size, ask_rate) = o;
        //
        double tokens_bought, funds_spent;
        std::tie(tokens_bought, funds_spent) =
            buy_nibble(spend_budget, fee_pc, fee_fix, ask_size, ask_rate);
        spend_budget -= funds_spent;
        if (spend_budget < 0)
        {
            std::cerr << "Error in arbitrage calculation - budget < 0" << std::endl;
            throw std::runtime_error("not a good thing");
        }
        double tokens_to_sell = tokens_bought;

        int multi_part_sell_index = 0;
        // just for debugging/summry info
        double multi_part_tokens_sold = 0;
        double multi_part_funds_received = 0;
        double multi_part_funds_spent = 0;

        // How much can we sell at the curent best rates ...
        while (sell_rate > ask_rate && tokens_to_sell > 0)
        {
            // Assuming we have bought on this exchange, how much can we sell on the other
            double tokens_sold, funds_received;
            std::tie(tokens_sold, funds_received) =
                sell_nibble(tokens_to_sell, fee_pc, fee_fix, sell_size, sell_rate);

            double gain = 0.0;
            double pc_r = 0.0;
            double tokens_bought_partial = tokens_bought;
            double funds_spent_partital  = funds_spent;

            // if we only sold some of the tokens, compute the parital purchase cost
            if (tokens_sold < tokens_bought)
            {
                std::tie(tokens_bought_partial, funds_spent_partital) =
                    buy_nibble(0, fee_pc, fee_fix, tokens_sold, ask_rate);
                // compute profit/loss
                gain = funds_received - funds_spent_partital;
                pc_r = 100.0 * (gain / funds_spent_partital);
            }
            else
            {
                gain = funds_received - funds_spent;
                pc_r = 100.0 * (gain / funds_spent);
            }
            trades.push_back(trade_set{
                                 tokens_bought, ask_size, ask_rate, funds_spent_partital,
                                 tokens_sold, sell_size, sell_rate, funds_received,
                                 gain, pc_r});

            if (multi_part_sell_index>0) {
                temp << num_partial % "---" % "---" % "---" % "---" %
                        tokens_sold % sell_size % sell_rate % funds_received %
                        gain % pc_r % cum_gain % cum_pc;
            }
            else {
                temp << num % tokens_bought % ask_size % ask_rate % funds_spent_partital %
                        tokens_sold % sell_size % sell_rate % funds_received %
                        gain % pc_r % cum_gain % cum_pc;
            }

            tokens_to_sell -= tokens_sold;
            sell_size -= tokens_sold;
            multi_part_tokens_sold += tokens_sold;
            multi_part_funds_received += funds_received;
            multi_part_funds_spent += funds_spent_partital;

            // if all sales at this price have been made, look at the next price slot
            // (should not be < 0 - numeric precision)
            if (sell_size<=0.0)
            {
                std::tie(sell_size, sell_rate) = *(++sell_point);
            }
            // end of a multi-part sale - display a summary
            if (tokens_to_sell<=0.0 && multi_part_sell_index>0) {
                gain = multi_part_funds_received - multi_part_funds_spent;
                pc_r = 100.0 * (gain / multi_part_funds_spent);
                //
                temp << num_summary % "---" % "---" % "---" % multi_part_funds_spent %
                        multi_part_tokens_sold % "---" % "---" % multi_part_funds_received %
                        gain % pc_r % cum_gain % cum_pc;
            }
            multi_part_sell_index++;
        }

        // always keep some small-change in the account
        if (spend_budget < 0.5)
            break;
    }
    // dump out the trade details
    if (trades.size() > 0)
    {
        std::cout << temp.str() << std::endl;
    }
    //
    return buy_here_sell_there;
}

// ----------------------------------------------------------------------------
// Bitstamp specific order book processing routines
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// accept json reply from bitstamp order book query and turn into numeric arrays
bool bitstamp_order_book::accept_json_bitstamp(std::string_view data)
{
    if (!startswith(data, "{\"data\":"))
        return false;
    nlohmann::json jdata = json::parse(data)["data"];
    //
    bids.clear();
    asks.clear();
    //
    // convert orders into a layout we can visualize nicely
    bid_ask_string_to_number(jdata["bids"], bids);
    bid_ask_string_to_number(jdata["asks"], asks);
    //
    std::partial_sum(bids.size.begin(), bids.size.end(), bids.total.begin());
    std::partial_sum(asks.size.begin(), asks.size.end(), asks.total.begin());

    // push this data into the graph object
    bid_curve_->setRawSamples_locked(bids.rate, bids.total);
    ask_curve_->setRawSamples_locked(asks.rate, asks.total);
    //
    order_text = order_book_string();
    //
    update_graph_limits();
    return true;
}

// ----------------------------------------------------------------------------
// bitstamp data arrives as strings instead of numbers
// these must be converted to numeric arrays
void bitstamp_order_book::bid_ask_string_to_number(nlohmann::json& json, offer_data& data)
{
    auto bid_string = json.get<std::array<std::array<std::string, 2>, 100>>();
    //
    data.rate.resize(bid_string.size(), 0);
    data.size.resize(bid_string.size(), 0);
    data.total.resize(bid_string.size(), 0);
    //
    std::transform(bid_string.begin(), bid_string.end(),
        ranges::view::zip(data.rate, data.size).begin(), [](const auto& i) {
#ifdef GROX_ARBITRAGE_TEST_MODE
            // increase the price on the exchange to test our buy/sell algorithm
            return std::pair<double, double>{std::stod(i[0]) + 0.015, std::stod(i[1])};
#else
            return std::pair<double, double>{ std::stod(i[0]), std::stod(i[1]) };
#endif
        });
}

// ----------------------------------------------------------------------------
// XRP ledger specific order book processing routines
// ----------------------------------------------------------------------------

// When subscribing to the ledger order book webstream
// a snapshot is inculded initially with the current state
// This function converts the json data into our order book form
// This function should only be executed once : when connecting to stream
void xrpl_order_book::accept_json_ledger_snapshot(std::string_view data)
{
    nlohmann::json jdata = json::parse(data);
    auto joffers = jdata["result"]["offers"];
    DEBUG_ONLY(joffers.dump(4) << std::endl);
    //
    // websocket (re?)connnect: clear the orderbook ...
    orders.clear();
    //
    auto offers = joffers.get<std::vector<xrpl_offer>>();
    //
    for (auto const& o : offers)
    {
        // skip unsupported currencies
        if (!o.grox_compatible()) {
            std::cerr << "Unsupported currency " << o << std::endl;
            continue;
        }
        // put offer into offer map
        insert_offer(o);
    }
    //
    ledger_map_to_order_book();
}

// This function converts an existing order book into
// plot and txt display forms
void xrpl_order_book::ledger_map_to_order_book()
{
    // make sure graph doesn't try to plot data during an update
    bid_curve_->clear_samples();
    ask_curve_->clear_samples();
    //
    bids.clear();
    asks.clear();
    //
    auto clamp_offers_to_funds =
            [](std::vector<xrpl_offer>& offers,
            currency_type curr,
            const std::string &acct)
    {
        // for debugging
        //std::stringstream temp;
        //temp << acct << " : Offers : ";
        //for (auto &o : offers) { temp << o.owner_funds << ", "; }
        //std::cout << temp.str() << std::endl;

        // the first offer always holds the max funds available
        double funds_avail = offers[0].owner_funds;
        if (funds_avail==-1) {
            throw std::runtime_error("bid fund tracking error");
        }
        for (auto &o : offers) {
            // used only in building bid/ask order books
            double amount = o.amount(curr);
            if (amount <= funds_avail) {
                o.funded_offer = amount;
            }
            else {
                o.funded_offer = std::max(0.0, funds_avail);
            }
            funds_avail -= amount;
        }
    };

    for (auto & [acct, bid_ask] : orders)
    {
        std::vector<xrpl_offer>& acc_bids = std::get<bid_index>(bid_ask);
        std::vector<xrpl_offer>& acc_asks = std::get<ask_index>(bid_ask);
        //
        DEBUG_ONLY(acct << " bids : " << bids.size() << " asks : " << asks.size());

        // the account may not be fully funded, so the offers may be invalid
        if (acc_bids.size()>0) {
            std::sort(acc_bids.begin(), acc_bids.end(), std::greater<xrpl_offer>{});
            clamp_offers_to_funds(acc_bids, currency_type::usd_bitstamp, acct);
        }
        double tiny_offers = 0;
        for (const auto& o : acc_bids)
        {
            auto xrp_amount = o.amount(currency_type::xrp) * 1E-6;
            // skip unfunded or very small offers
            if (o.unfunded(0.1)) {
                tiny_offers += xrp_amount;
                continue;
            }
            //
            bids.rate.push_back(o.rate());
            bids.orig.push_back(xrp_amount + tiny_offers);
            bids.size.push_back(o.funded_offer / o.rate());
            tiny_offers = 0;
        }

        if (acc_asks.size()>0) {
            std::sort(acc_asks.begin(), acc_asks.end(), std::less<xrpl_offer>{});
            clamp_offers_to_funds(acc_asks, currency_type::xrp, acct);
        }
        tiny_offers = 0;
        for (const auto& o : acc_asks)
        {
            auto xrp_amount = o.amount(currency_type::xrp) * 1E-6;
            // skip unfunded or very small offers
            if (o.unfunded(0.1)) {
                tiny_offers += xrp_amount;
                continue;
            }
            // skip unfunded or very small offers
            if (o.unfunded(0.1 * 1E6)) continue;
            //
            asks.rate.push_back(o.rate());
            asks.orig.push_back(xrp_amount + tiny_offers);
            asks.size.push_back(o.funded_offer * 1E-6);
            tiny_offers = 0;
        }
    }

    // sort zipped X/Y bids from high to low, sort based on rate
    ranges::sort(ranges::views::zip(bids.rate, bids.size, bids.orig),
        [](auto&& a, auto&& b) { return std::get<0>(a) > std::get<0>(b); });

    // sort zipped X/Y asks from low to high, sort based on X=conv
    ranges::sort(ranges::views::zip(asks.rate, asks.size, asks.orig),
        [](auto&& a, auto&& b) { return std::get<0>(a) < std::get<0>(b); });

    // partial sum the bids
    bids.total.resize(bids.size.size());
    std::partial_sum(bids.size.begin(), bids.size.end(), bids.total.begin());

    // partial sum the asks
    asks.total.resize(asks.size.size());
    std::partial_sum(asks.size.begin(), asks.size.end(), asks.total.begin());

    // push this data into the graph object
    bid_curve_->setRawSamples_locked(bids.rate, bids.total);
    ask_curve_->setRawSamples_locked(asks.rate, asks.total);
    //
    order_text = order_book_string();
}

void xrpl_order_book::accept_json_ledger_transaction(std::string_view data)
{
    nlohmann::json jdata = json::parse(data);
    std::string success = jdata.at("engine_result").get<std::string>();
    if (success != "tesSUCCESS")
        return;
    //
    nlohmann::json affected = jdata["meta"]["AffectedNodes"];
    DEBUG_ONLY(affected.dump(4) << std::endl);

    nlohmann::json transaction = jdata["transaction"];
    DEBUG_ONLY(transaction.dump(4) << std::endl);

    std::string ttype = transaction.at("TransactionType").get<std::string>();
    if (ttype == "OfferCreate" || ttype == "OfferCancel" || ttype == "Payment")
    {
        try
        {
            handle_offer_change(transaction, affected);
        }
        catch (std::exception& e)
        {
            std::cerr << "Error : Transaction : " << transaction.dump(4) << std::endl;
            std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
            throw e;
        }
    }
    else
    {
        std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
        std::cerr << "Error : Transaction type : " << transaction.dump(4) << std::endl;
        throw std::runtime_error("new transaction type : " + ttype);
    }
    ledger_map_to_order_book();
}

bool xrpl_order_book::update_offer(
    const xrpl_offer& prev_offer, xrpl_offer& final_offer, double owner_funds)
{
    const std::string& acct = prev_offer.Account;
    offer_map::iterator it = orders.find(acct);
    // if not in map
    if (it == orders.end())
    {
        std::cerr << prev_offer.Account << " update_offer address not in map"
                  << std::endl;
        return false;
    }
    //
    std::vector<xrpl_offer>& acc_bids = std::get<bid_index>(it->second);
    std::vector<xrpl_offer>& acc_asks = std::get<ask_index>(it->second);
    if (prev_offer.TakerPays.currency == currency_type::xrp)
    {
        auto it2 = std::find(acc_bids.begin(), acc_bids.end(), prev_offer);
        if (it2 == acc_bids.end())
        {
            std::cerr << prev_offer.Account << " update_offer bid not found" << std::endl;
            return false;
        }
        // to enable fund tracking
        if (it2->owner_funds!=-1 && final_offer.owner_funds==-1) {
            final_offer.owner_funds = it2->owner_funds;
        }
        // overwrite old offer with new one
        DEBUG_ONLY("Update Bid: " << prev_offer << " " << final_offer);
        *it2 = final_offer;
    }
    else
    {
        auto it2 = std::find(acc_asks.begin(), acc_asks.end(), prev_offer);
        if (it2 == acc_asks.end())
        {
            std::cerr << prev_offer.Account << " update_offer ask not found" << std::endl;
            return false;
        }
        // overwrite old offer with new one
        DEBUG_ONLY("Update Ask: " << prev_offer << " " << final_offer);
        *it2 = final_offer;
    }
    return true;
}

bool xrpl_order_book::insert_offer(const xrpl_offer& offer)
{
    const std::string& acct = offer.Account;
    offer_map::iterator it = orders.find(acct);
    // if not in map, create new entry
    if (it == orders.end())
    {
        account_bid_ask_data bid_ask{ {}, {} };
        const auto [it2, success] = orders.insert({offer.Account, bid_ask});
        if (success)
            it = it2;
        else
        {
            std::cerr << offer.Account << " insert_offer map insert error" << std::endl;
            return false;
        }
    }
    // add new order to map vectors
    std::vector<xrpl_offer>& acc_bids = std::get<bid_index>(it->second);
    std::vector<xrpl_offer>& acc_asks = std::get<ask_index>(it->second);
    if (offer.TakerPays.currency == currency_type::xrp)
    {
        acc_bids.push_back(offer);
        DEBUG_ONLY("Insert Bid: " << offer);
    }
    else
    {
        acc_asks.push_back(offer);
        DEBUG_ONLY("Insert Ask: " << offer);
    }
    return true;
}

bool xrpl_order_book::delete_offer(const xrpl_offer& offer)
{
    const std::string& acct = offer.Account;
    offer_map::iterator it = orders.find(acct);
    // if not in map
    if (it == orders.end())
    {
        std::cerr << offer.Account << " delete_offer address not in map" << std::endl;
        return false;
    }
    // remove order from map vector
    std::vector<xrpl_offer>& acc_bids = std::get<bid_index>(it->second);
    std::vector<xrpl_offer>& acc_asks = std::get<ask_index>(it->second);
    if (offer.TakerPays.currency == currency_type::xrp)
    {
        auto val = std::find(acc_bids.begin(), acc_bids.end(), offer);
        if (val == acc_bids.end())
        {
            std::cerr << "Error : Bid delete not found" << std::endl;
            std::cerr << "Bid: " << offer << std::endl;
            return false;
        }
        else if (val!=std::prev(acc_bids.end()))
        {
            // update tracking of account funds
            if (val->owner_funds!=-1) {
                DEBUG_ONLY("Update owner_funds " << val->owner_funds);
                std::next(val)->owner_funds = val->owner_funds;
            }
        }
        DEBUG_ONLY("Delete Bid: " << offer);
        acc_bids.erase(val);
    }
    else
    {
        auto val = std::find(acc_asks.begin(), acc_asks.end(), offer);
        if (val == acc_asks.end())
        {
            std::cerr << "Ask: " << offer << std::endl;
            std::cerr << "Error : Ask delete not found" << std::endl;
            return false;
        }
        else if (val!=std::prev(acc_asks.end()))
        {
            // update tracking of account funds
            if (val->owner_funds!=-1) {
                DEBUG_ONLY("Update owner_funds " << val->owner_funds);
                std::next(val)->owner_funds = val->owner_funds;
            }
        }
        DEBUG_ONLY("Delete Ask: " << offer);
        acc_asks.erase(val);
    }
    if (acc_bids.size() == 0 && acc_asks.size() == 0)
    {
        // we can safely remove the account
        DEBUG_ONLY("Account " << offer.Account << " can be removed");
        orders.erase(offer.Account);
    }
    return true;
}

enum node_edit
{
    created = 0,
    modified,
    deleted
};

void xrpl_order_book::handle_offer_change(
    const nlohmann::json& trans, const nlohmann::json& affected)
{
    bool ok = true;
    for (auto& el : affected.items())
    {
        const nlohmann::json* node;
        node_edit edit_type;

        // 3 types that affect out order book
        if (el.value().contains("CreatedNode"))
        {
            node = &el.value()["CreatedNode"];
            edit_type = node_edit::created;
        }
        else if (el.value().contains("ModifiedNode"))
        {
            node = &el.value()["ModifiedNode"];
            edit_type = node_edit::modified;
        }
        else if (el.value().contains("DeletedNode"))
        {
            node = &el.value()["DeletedNode"];
            edit_type = node_edit::deleted;
        }
        else
        {
            // don't process other node types
            continue;
        }

        //
        std::string ltype = (*node)["LedgerEntryType"].get<std::string>();
        if (ltype != "Offer")
        {
            // don't process other node types
            continue;
        }

        // NB. grox_compatible = only xrp<==>usd_bitstamp
        xrpl_offer final_offer, prev_offer;
        if (node->contains("NewFields"))
        {
            final_offer = (*node)["NewFields"].get<xrpl_offer>();
            if (!final_offer.grox_compatible())
                continue;
        }
        if (node->contains("FinalFields"))
        {
            final_offer = (*node)["FinalFields"].get<xrpl_offer>();
            if (!final_offer.grox_compatible())
                continue;
        }
        if (node->contains("PreviousFields"))
        {
            prev_offer = (*node)["FinalFields"].get<xrpl_offer>();
            if (!prev_offer.grox_compatible())
                continue;
        }

        // To track unfunded offers, we add the "owner_funds" as it will
        // appear in the transaction, but not the "offer" node,
        // (except when receeiving initial order book)
        double owner_funds = -1;
        if (trans.contains("owner_funds")) {
            owner_funds = std::stod(trans.at("owner_funds").get< std::string >());
            final_offer.owner_funds = owner_funds;
        }

        switch (edit_type)
        {
        case node_edit::created:
            ok &= insert_offer(final_offer);
            break;
        case node_edit::modified:
            ok &= update_offer(prev_offer, final_offer);
            break;
        case node_edit::deleted:
            ok &= delete_offer(final_offer);
            break;
        }
    }
    if (!ok)
    {
        std::cerr << "Error : Transaction : " << trans.dump(4) << std::endl;
        std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
        throw std::runtime_error("Error in handle_offer_change");
    }
}
