#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <qwt_scale_div.h>
//
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <list>
//
#include <boost/iterator/zip_iterator.hpp>
#include <boost/format.hpp>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
//
#include "ohlc.hpp"
#include "nlohmann/json.hpp"
//
#include "plots/OrderBookPlot.h"
#include "plots/OrderBookCurve.h"
//
#ifndef DEBUG_ONLY
# define DEBUG_ONLY(x)
#endif

bool startswith(const std::string_view str, const std::string &sub);
//
using bid_ask_vectors = std::pair<std::vector<xrpl_offer>, std::vector<xrpl_offer>>;
using offer_map = std::unordered_map<std::string, bid_ask_vectors>;
using offer_pair = std::pair<std::string, bid_ask_vectors>;
//
// ----------------------------------------------------------------------------
// Orders are processed into simple lists of amount, running total
// ----------------------------------------------------------------------------
struct offer_data {
    std::vector<float> rate;
    std::vector<float> size;
    std::vector<float> total;
    //
    void clear() {
        rate.clear();
        size.clear();
        total.clear();
    }
};

// ----------------------------------------------------------------------------
// Base order book class provides access to top bids/asks
// plotting and other representations of the orders
// ----------------------------------------------------------------------------
struct order_book_base
{
    // Sorted order book entries
    offer_data bids;
    offer_data asks;

    // Graph plotting objects
    std::shared_ptr<OrderBookPlot> OrderBookPlot_;
    OrderBookCurve *bid_curve_;
    OrderBookCurve *ask_curve_;

    // Graph min/max setting
    double prev_xmin;
    double prev_xmax;
    double prev_ymax;

    // Text representation of order book
    std::string order_text;
    //
    order_book_base(std::shared_ptr<OrderBookPlot> obp, bool secondaxis)
    {
        OrderBookPlot_ = obp;
        //
        prev_xmin = 0;
        prev_xmax = 0;
        prev_ymax = 0;
        //
        bid_curve_ = new OrderBookCurve();
        ask_curve_ = new OrderBookCurve();
        //
        if (secondaxis) {
            bid_curve_->setSegmentInfo(0, 100, Qt::darkYellow, 3);
            ask_curve_->setSegmentInfo(0, 100, Qt::darkMagenta,3);
            bid_curve_->setYAxis(QwtPlot::yRight);
            ask_curve_->setYAxis(QwtPlot::yRight);
        }
        else {
            bid_curve_->setSegmentInfo(0, 100, Qt::green, 3);
            ask_curve_->setSegmentInfo(0, 100, Qt::red,3);
            bid_curve_->setYAxis(QwtPlot::yLeft);
            ask_curve_->setYAxis(QwtPlot::yLeft);
        }
        bid_curve_->attach(obp.get());
        ask_curve_->attach(obp.get());
    }

    ~order_book_base()
    {
        // release shared_ptr reference early
        OrderBookPlot_ = nullptr;

        // owned by plot?
        // delete plot_curve_;
    }

    void update_graph_limits()
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
        if (first_time) {
            prev_xmin = xmin;
            prev_xmax = xmax;
            prev_ymax = ymax;
            first_time = false;
        }
        else {
            double x1 = (xmin-prev_xmin)/50.0;
            double x2 = (prev_xmax-xmax)/50.0;
            double y2 = (prev_ymax-ymax)/50.0;
            prev_xmin += x1;
            prev_xmax -= x2;
            prev_ymax -= y2;
        }
        //
        OrderBookPlot_->setAxisScale(QwtPlot::xBottom, prev_xmin, prev_xmax);
        OrderBookPlot_->setAxisScale(QwtPlot::yLeft,  ymin, prev_ymax);
        OrderBookPlot_->setAxisScale(QwtPlot::yRight, ymin, prev_ymax/5.0);
    }

    // produces a simple string representation of the order book
    // from the bid/ask lists
    std::string order_book_string()
    {
        // string header line
        std::stringstream temp;
        // title format string
        boost::format title("%10s %10s %10s | %10s %10s %10s\n");
        temp << title % "Total" % "Size" % "Bid" % "Ask" % "Size" % "Total";
        // numeric entries format string
        boost::format num("%10.0f %10.2f %10.4f | %10.4f %10.2f %10.0f\n");
        // iterate over bids/asks
        auto zipped = ranges::view::zip(bids.total, bids.size, bids.rate, asks.rate, asks.size, asks.total);
        for (auto const & z : zipped)
        {
            temp << num % std::get<0>(z) % std::get<1>(z) % std::get<2>(z) % std::get<3>(z) % std::get<4>(z) % std::get<5>(z);
        }
        //
        return temp.str();
    }
};

// ----------------------------------------------------------------------------
// Bitstamp specific order book processing routines
// ----------------------------------------------------------------------------
struct bitstamp_order_book : order_book_base
{
    using order_book_base::order_book_base;

    // ----------------------------------------------------------------------------
    // accept json reply from bitstamp order book query and turn into numeric arrays
    bool accept_json_bitstamp(std::string_view data)
    {
        if (!startswith(data, "{\"data\":")) return false;
        nlohmann::json jdata = json::parse(data)["data"];
        //
        // convert orders into a layout we can visualize nicely
        bid_ask_string_to_number(jdata["bids"], bids);
        bid_ask_string_to_number(jdata["asks"], asks);
        //
        std::partial_sum(bids.size.begin(), bids.size.end(), bids.total.begin());
        std::partial_sum(asks.size.begin(), asks.size.end(), asks.total.begin());

        // push this data into the graph object
        bid_curve_->setRawSamples(&bids.rate[0], &bids.total[0], 100);
        ask_curve_->setRawSamples(&asks.rate[0], &asks.total[0], 100);
        //
        update_graph_limits();
        return true;
    }

private:
    // ----------------------------------------------------------------------------
    // bitstamp data arrives as strings instead of numbers
    // these must be converted to numeric arrays
    void bid_ask_string_to_number(nlohmann::json& json, offer_data &data)
    {
        auto bid_string = json.get<std::array<std::array<std::string, 2>, 100>>();
        //
        data.rate.resize(bid_string.size(), 0);
        data.size.resize(bid_string.size(), 0);
        data.total.resize(bid_string.size(), 0);
        //
        std::transform(bid_string.begin(), bid_string.end(), ranges::view::zip(data.rate, data.size).begin(), [](const auto& i) {
            return std::pair<double, double>{ std::stod(i[0]), std::stod(i[1]) };
        });
    }
};

// ----------------------------------------------------------------------------
// XRP ledger specific order book processing routines
// ----------------------------------------------------------------------------
struct xrpl_order_book : order_book_base
{
    using order_book_base::order_book_base;
    //
    offer_map orders;
    //
    xrpl_order_book(std::shared_ptr<OrderBookPlot> obp, bool secondaxis)
        : order_book_base(obp, secondaxis)
    {
    }

    // When subscribing to the ledger order book webstream
    //  a snapshot is inculded initiall with the current state
    // This function converts the json into our order book
    void accept_json_ledger_snapshot(std::string_view data)
    {
        nlohmann::json jdata = json::parse(data);
        auto joffers = jdata["result"]["offers"];
        DEBUG_ONLY(joffers.dump(4) << std::endl);
        //
        auto offers = joffers.get<std::vector<xrpl_offer>>();
        //
        bids.clear();
        asks.clear();
        //
        double btotal = 0;
        double atotal = 0;
        for (auto const &o : offers) {

            // build viz of bids/asks
            double xrp, usd, conv;
            // bid for xrp, taker gets $usd
            if (o.TakerPays.currency==currency_type::xrp) {
                xrp = o.TakerPays.value*1E-6;
                usd = o.TakerGets.value;
                // reject unfunded offers
                if (xrp==0 || usd==0) continue;
                //
                conv = usd / xrp;
                btotal += xrp;
                bids.rate.push_back(conv);
                bids.size.push_back(xrp);
                bids.total.push_back(btotal);
            }
            // ask for $usd in return for xrp
            else if (o.TakerPays.currency==currency_type::usd_bitstamp) {
                xrp = o.TakerGets.value*1E-6;
                usd = o.TakerPays.value;
                // reject unfunded offers
                if (xrp==0 || usd==0) continue;
                //
                conv = usd / xrp;
                atotal += xrp;
                asks.rate.push_back(conv);
                asks.size.push_back(xrp);
                asks.total.push_back(atotal);
            }
            else {
                throw std::runtime_error("Unsupported currency");
            }

            // put offer into offer map
            insert_offer(o);

            DEBUG_ONLY("rate : " << std::setw(10) << std::setprecision(7) << conv << "\t"
                      << o.Account << "\t"
                      << std::setw(12) << std::setprecision(11) << xrp << " "
                      << "$" << std::setw(12) << std::setprecision(11) << usd);
        }
/*
        // take the top 100 xrp buy orders and transform them to a plot format
        // partial sum the first 100 buy/bids, store in z array reverse order
        std::array<double, 200>::reverse_iterator revx(&graph_x[100]);
        std::array<double, 200>::reverse_iterator revz(&graph_z[100]);
        std::copy(&bid_x[0], &bid_x[100], revx);
        std::partial_sum(&bid_y[0], &bid_y[100], revz);

        // copy the ask/sell data directly
        std::copy(&ask_x[0], &ask_x[100], &graph_x[100]);
        std::partial_sum(&ask_y[0], &ask_y[100], &graph_z[100]);
*/
        // push this data into the graph object
        bid_curve_->setRawSamples(&bids.rate[0], &bids.total[0], 100);
        ask_curve_->setRawSamples(&asks.rate[0], &asks.total[0], 100);
    }

    void ledger_map_to_plot()
    {
        std::vector<double> bid_x, bid_y;
        std::vector<double> ask_x, ask_y;
        //
        bid_x.reserve(600);
        bid_y.reserve(600);
        ask_x.reserve(600);
        ask_y.reserve(600);
        //
        // build viz of bids/asks
        double xrp, usd, conv;
        //
        for (auto const& [acct, bid_ask] : orders)
        {
            const std::vector<xrpl_offer> &acc_bids = bid_ask.first;
            const std::vector<xrpl_offer> &acc_asks = bid_ask.second;
            //
            DEBUG_ONLY(acct
                        << " bids : " << bids.size()
                        << " asks : " << asks.size());

            for (const auto &o : acc_bids) {
                xrp = o.TakerPays.value*1E-6;
                usd = o.TakerGets.value;
                conv = usd / xrp;
                bid_x.push_back(conv);
                bid_y.push_back(xrp);
            }
            for (const auto &o : acc_asks) {
                xrp = o.TakerGets.value*1E-6;
                usd = o.TakerPays.value;
                conv = usd / xrp;
                ask_x.push_back(conv);
                ask_y.push_back(xrp);
            }
        }

        // sort zipped X/Y bids from high to low, sort based on X=conv
        ranges::sort(ranges::view::zip(bid_x, bid_y), [](auto &&a, auto &&b) {
            return std::get<0>(a) > std::get<0>(b);
        });

        // sort zipped X/Y asks from low to high, sort based on X=conv
        ranges::sort(ranges::view::zip(ask_x, ask_y), [](auto &&a, auto &&b) {
            return std::get<0>(a) < std::get<0>(b);
        });

        // take the top 100 xrp buy orders and transform them to a plot format
        // partial sum the first 100 buy/bids, store in z array reverse order
        std::copy(&bid_x[0], &bid_x[100], bids.rate.begin());
        std::partial_sum(&bid_y[0], &bid_y[100], bids.total.begin());

        // copy the ask/sell data directly
        std::copy(&ask_x[0], &ask_x[100], asks.rate.begin());
        std::partial_sum(&ask_y[0], &ask_y[100], asks.total.begin());

        order_text = order_book_string();

        // push this data into the graph object
        bid_curve_->setRawSamples(&bids.rate[0], &bids.total[0], 100);
        ask_curve_->setRawSamples(&asks.rate[0], &asks.total[0], 100);
    }

    void accept_json_ledger_transaction(std::string_view data)
    {
        nlohmann::json jdata = json::parse(data);
        std::string success = jdata.at("engine_result").get< std::string >();
        if (success != "tesSUCCESS") return;
        //
        nlohmann::json affected = jdata["meta"]["AffectedNodes"];
        DEBUG_ONLY(affected.dump(4) << std::endl);

        nlohmann::json transaction = jdata["transaction"];
        DEBUG_ONLY(transaction.dump(4) << std::endl);

        std::string ttype = transaction.at("TransactionType").get< std::string >();
        if (ttype=="OfferCreate" || ttype=="OfferCancel" || ttype=="Payment")
        {
            try {
                handle_offer_change(transaction, affected);
            }
            catch (std::exception& e)
            {
                std::cerr << "Error : Transaction : " << transaction.dump(4) << std::endl;
                std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
                throw e;
            }
        }
        else {
            std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
            std::cerr << "Error : Transaction type : " << transaction.dump(4) << std::endl;
            throw std::runtime_error("new transaction type : " + ttype);
        }
        ledger_map_to_plot();
    }

    bool update_offer(const xrpl_offer &prev_offer, const xrpl_offer &final_offer)
    {
        const std::string &acct = prev_offer.Account;
        offer_map::iterator it = orders.find(acct);
        // if not in map
        if (it == orders.end()) {
            std::cerr << prev_offer.Account << " update_offer address not in map" << std::endl;
            return false;
        }
        //
        std::vector<xrpl_offer> &bids = it->second.first;
        std::vector<xrpl_offer> &asks = it->second.second;
        if (prev_offer.TakerPays.currency==currency_type::xrp) {
            auto it2 = std::find(bids.begin(), bids.end(), prev_offer);
            if (it2 == bids.end()) {
                std::cerr << prev_offer.Account << " update_offer bid not found" << std::endl;
                return false;
            }
            // overwrite old offer with new one
            std::cout << "Update Bid: " << prev_offer << " " << final_offer << std::endl;
            *it2 = final_offer;
        }
        else {
            auto it2 = std::find(asks.begin(), asks.end(), prev_offer);
            if (it2 == asks.end()) {
                std::cerr << prev_offer.Account << " update_offer ask not found" << std::endl;
                return false;
            }
            // overwrite old offer with new one
            std::cout << "Update Ask: " << prev_offer << " " << final_offer << std::endl;
            *it2 = final_offer;
        }
        return true;
    }

    bool insert_offer(const xrpl_offer &offer)
    {
        const std::string &acct = offer.Account;
        offer_map::iterator it = orders.find(acct);
        // if not in map, create new entry
        if (it == orders.end()) {
            bid_ask_vectors bid_ask;
            const auto [it2, success] = orders.insert({offer.Account, bid_ask});
            if (success) it = it2;
            else {
                std::cerr << offer.Account << " insert_offer map insert error" << std::endl;
                return false;
            }
        }
        // add new order to map vector
        std::vector<xrpl_offer> &bids = it->second.first;
        std::vector<xrpl_offer> &asks = it->second.second;
        if (offer.TakerPays.currency==currency_type::xrp) {
            bids.push_back(offer);
            std::cout << "Insert Bid: " << offer << std::endl;
        }
        else {
            asks.push_back(offer);
            std::cout << "Insert Ask: " << offer << std::endl;
        }
        return true;
    }

    bool delete_offer(const xrpl_offer &offer)
    {
        const std::string &acct = offer.Account;
        offer_map::iterator it = orders.find(acct);
        // if not in map
        if (it == orders.end()) {
            std::cerr << offer.Account << " delete_offer address not in map" << std::endl;
            return false;
        }
        // remove order from map vector
        std::vector<xrpl_offer> &bids = it->second.first;
        std::vector<xrpl_offer> &asks = it->second.second;
        if (offer.TakerPays.currency==currency_type::xrp) {
            auto val = std::find(bids.begin(), bids.end(), offer);
            if (val == bids.end()) {
                std::cerr << "Error : Bid delete not found" << std::endl;
                std::cerr << "Bid: " << offer << std::endl;
                return false;
            }
            std::cout << "Delete Bid: " << offer << std::endl;
            bids.erase(val);
        }
        else {
            auto val = std::find(asks.begin(), asks.end(), offer);
            if (val == asks.end()) {
                std::cerr << "Ask: " << offer << std::endl;
                std::cerr << "Error : Ask delete not found" << std::endl;
                return false;
            }
            std::cout << "Delete Ask: " << offer << std::endl;
            asks.erase(val);
        }
        if (bids.size()==0 && asks.size()==0) {
            // we can safely remove the account
            DEBUG_ONLY("Account " << offer.Account << " can be removed");
            orders.erase(offer.Account);
        }
        return true;
    }

    enum node_edit {
        created=0,
        modified,
        deleted
    };

    void handle_offer_change(const nlohmann::json &trans, const nlohmann::json &affected)
    {
        bool ok = true;
        for (auto& el : affected.items())
        {
            const nlohmann::json *node;
            node_edit edit_type;

            // 3 types that affect out order book
            if (el.value().contains("CreatedNode")) {
                node = &el.value()["CreatedNode"];
                edit_type = node_edit::created;
            }
            else if (el.value().contains("ModifiedNode")) {
                node = &el.value()["ModifiedNode"];
                edit_type = node_edit::modified;
            }
            else if (el.value().contains("DeletedNode")) {
                node = &el.value()["DeletedNode"];
                edit_type = node_edit::deleted;
            }
            else {
                // don't process other node types
                continue;
            }

            //
            std::string ltype = (*node)["LedgerEntryType"].get< std::string >();
            if (ltype!="Offer") {
                // don't process other node types
                continue;
            }

            // NB. grox_compatible = only xrp<==>usd_bitstamp
            xrpl_offer final_offer, prev_offer;
            if (node->contains("NewFields")) {
                final_offer = (*node)["NewFields"].get< xrpl_offer >();
                if (!final_offer.grox_compatible()) continue;
            }
            if (node->contains("FinalFields")) {
                final_offer = (*node)["FinalFields"].get< xrpl_offer >();
                if (!final_offer.grox_compatible()) continue;
            }
            if (node->contains("PreviousFields")) {
                prev_offer = (*node)["FinalFields"].get< xrpl_offer >();
                if (!prev_offer.grox_compatible()) continue;
            }

            if (trans.contains("owner_funds")) {
                double owner_funds = std::stod(trans.at("owner_funds").get< std::string >());
            }

            switch (edit_type) {
                case node_edit::created:
                    ok &= insert_offer(final_offer); break;
                case node_edit::modified:
                    ok &= update_offer(prev_offer, final_offer); break;
                case node_edit::deleted:
                    ok &= delete_offer(final_offer); break;
            }
        }
        if (!ok) {
            std::cerr << "Error : Transaction : " << trans.dump(4) << std::endl;
            std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
            throw std::runtime_error("Error in handle_offer_change");
        }
    }

};
