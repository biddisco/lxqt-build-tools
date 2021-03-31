#pragma once

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
#ifndef DEBUG_ONLY
#define DEBUG_ONLY(x)
#endif

//#define GROX_TEST_MODE 1

bool startswith(const std::string_view str, const std::string& sub);
//
constexpr static int bid_index = 0;
constexpr static int ask_index = 1;
using account_bid_ask_data = std::tuple<std::vector<xrpl_offer>, std::vector<xrpl_offer>>;
using offer_map = std::unordered_map<std::string, account_bid_ask_data>;
using offer_pair = std::pair<std::string, account_bid_ask_data>;
//
// ----------------------------------------------------------------------------
// Orders are processed into simple lists of amount, running total
// ----------------------------------------------------------------------------
struct offer_data
{
    std::vector<float> rate;
    std::vector<float> size;
    std::vector<float> orig;
    std::vector<float> total;
    //
    void clear()
    {
        rate.clear();
        size.clear();
        orig.clear();
        total.clear();
    }
};

struct fee_data
{
    // a percentage charged on every transaction
    double percent;
    // a fixed amount taken for a transaction
    double fixed;
};

// ----------------------------------------------------------------------------
// Base order book class provides access to top bids/asks
// plotting and other representations of the orders
// ----------------------------------------------------------------------------
struct order_book_base
{
    using trade_set = std::tuple<double, double, double, double, double, double, double,
        double, double, double>;
    using arb_vector = std::vector<trade_set>;

    // Sorted order book entries
    offer_data bids;
    offer_data asks;

    // Graph plotting objects
    std::shared_ptr<OrderBookPlot> OrderBookPlot_;
    OrderBookCurve* bid_curve_;
    OrderBookCurve* ask_curve_;

    // Graph min/max control
    double prev_xmin;
    double prev_xmax;
    double prev_ymax;

    // Text representation of order book
    std::string order_text;

    // construct, passing plot object in
    order_book_base(std::shared_ptr<OrderBookPlot> obp, bool secondaxis);

    // clean up
    ~order_book_base();

    void update_graph_limits();

    // produces a simple string representation of the order book
    // from the bid/ask lists
    std::string order_book_string();

    // given a max amount to spend, how much of this ask to take
    std::pair<double, double> buy_nibble(double max_spend, double fee_percent,
        double fee_fixed, double size, double rate) const;

    // given some tokens to sell, how much of this bid to take
    std::pair<double, double> sell_nibble(double max_tokens, double fee_percent,
        double fee_fixed, double size, double rate) const;

    // given another orderbook, if we buy on this one and sell on the other
    // are there arbitrage opportunities between the two
    arb_vector compute_arbitrage(order_book_base const& other,
        double budget, fee_data buy_fee, fee_data sell_fee,
            double test_offset, std::string &string_output) const;
};

// ----------------------------------------------------------------------------
// Bitstamp specific order book processing routines
// ----------------------------------------------------------------------------
struct bitstamp_order_book : order_book_base
{
    using order_book_base::order_book_base;

    // ----------------------------------------------------------------------------
    // accept json reply from bitstamp order book query and turn into numeric arrays
    bool accept_json_bitstamp(std::string_view data);

private:
    // ----------------------------------------------------------------------------
    // bitstamp data arrives as strings instead of numbers
    // these must be converted to numeric arrays
    void bid_ask_string_to_number(nlohmann::json& json, offer_data& data);
};

// ----------------------------------------------------------------------------
// XRP ledger specific order book processing routines
// ----------------------------------------------------------------------------
struct xrpl_order_book : order_book_base
{
    using order_book_base::order_book_base;
    //
    offer_map orders;

    // When subscribing to the ledger order book webstream
    //  a snapshot is inculded initiall with the current state
    // This function converts the json into our order book
    void accept_json_ledger_snapshot(std::string_view data);

    void ledger_map_to_order_book();

    void accept_json_ledger_transaction(std::string_view data);

    bool update_offer(const xrpl_offer& prev_offer, xrpl_offer& final_offer, double owner_funds=-1);

    bool insert_offer(const xrpl_offer& offer);

    bool delete_offer(const xrpl_offer& offer);

    enum node_edit
    {
        created = 0,
        modified,
        deleted
    };

    void handle_offer_change(const nlohmann::json& trans, const nlohmann::json& affected);
};
