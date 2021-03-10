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
// order_book
// ----------------------------------------------------------------------------

class order_book {
  public:
    //
    std::shared_ptr<OrderBookPlot> OrderBookPlot_;
    OrderBookCurve *plot_curve_;
    //
    std::array<double, 200> data_x;
    std::array<double, 200> data_y;
    std::array<double, 200> data_z;
    //
    double prev_xmin;
    double prev_xmax;
    double prev_ymax;
    //
    offer_map orders;
    std::string order_text;
    //
    order_book(std::shared_ptr<OrderBookPlot> obp, bool secondaxis)
    {
        OrderBookPlot_ = obp;
        //
        std::fill(std::begin(data_x), std::end(data_x), 0);
        std::fill(std::begin(data_y), std::end(data_y), 0);
        std::fill(std::begin(data_z), std::end(data_z), 0);
        //
        prev_xmin = 0;
        prev_xmax = 0;
        prev_ymax = 0;
        //
        plot_curve_ = new OrderBookCurve();
        //
        if (secondaxis) {
            plot_curve_->setSegmentInfo(0  , 100, Qt::darkYellow, 3);
            plot_curve_->setSegmentInfo(100, 200, Qt::darkMagenta,3);
            plot_curve_->setYAxis(QwtPlot::yRight);
        }
        else {
            plot_curve_->setSegmentInfo(0  , 100, Qt::green, 2);
            plot_curve_->setSegmentInfo(100, 200, Qt::red, 2);
        }
        plot_curve_->attach(obp.get());
    }

    ~order_book() {
        // release shared_ptr reference
        OrderBookPlot_ = nullptr;
    }
    //
    void set_raw_samples(const std::array<double, 200> &x, const std::array<double, 200> y)
    {
    // mw->OrderBookPlot_->plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
    }

    void update_graph_limits()
    {
        // pick x min max limits so they don't jump around constantly
        double xrange = data_x[199] - data_x[0];
        double xscale = 0.25 * std::pow(10, static_cast<int>(std::log10(xrange)));
        double xmin = std::floor(data_x[0] / xscale) * xscale;
        double xmax = std::ceil(data_x[199] / xscale) * xscale;
        //
        // pick y min max limits so they don't jump around constantly
        double yrange = std::max(data_z[0], data_z[199]);
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

    // ----------------------------------------------------------------------------
    // accept json reply from bitstamp order book query and turn into numeric arrays
    void accept_json_bitstamp(std::string_view data)
    {
        if (!startswith(data, "{\"data\":")) return;
        //
        nlohmann::json jdata = json::parse(data)["data"];

        // convert orders into a layout we can visualize nicely
        bid_ask_string_to_number(jdata["bids"], &data_x[0],   &data_y[0]);
        bid_ask_string_to_number(jdata["asks"], &data_x[100], &data_y[100]);
        // partial sum the 100 asks and bids, store in new y axis z array (left/bids part in reverse order)
        std::partial_sum(
            &data_y[0], &data_y[100], std::reverse_iterator<double*>(&data_z[100]));
        std::partial_sum(&data_y[100], &data_y[200], &data_z[100]);
        // and flip the x axis for the bids/left side of plot
        std::reverse(&data_x[0], &data_x[100]);

        // push this data intp the graph object
        plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
        //
        update_graph_limits();
    }

    // this function is no longer needed, but kept for future use
    void accept_json_ledger_sell(std::string_view data)
    {
        return;

        if (!startswith(data, "{\"result\":")) return;
        //
        nlohmann::json jdata = json::parse(data);
        auto joffers = jdata["result"]["offers"];
        //
        std::cout << "\n\nXRP Sell orders \n\n";
        //
        int index = 100;
        auto offers = joffers.get<std::vector<xrpl_offer>>();
        for (auto const &o : offers) {
            double xrp = o.TakerGets.value*1E-6;
            double usd = o.TakerPays.value;
            double conv = usd / xrp;
            std::cout << "rate : " << std::setw(10) << std::setprecision(7) << conv << "\t"
                      << o.Account << "\t"
                      << std::setw(10) << std::setprecision(11) << xrp << " "
                      << "$" << std::setw(10) << std::setprecision(11) << usd << std::endl;
            //
            data_x[index] = conv;
            data_y[index] = xrp;
            if (++index>=200) break;
        }
        // pad out in case there were less than 100 entries in the json
        for (; index<200; ++index) {
            data_x[index] = data_x[index-1];
            data_y[index] = 0;
        }
        // partial sum the 100 asks and bids, store in new y axis z array (left/bids part in reverse order)
        std::partial_sum(&data_y[100], &data_y[200], &data_z[100]);
        std::partial_sum(
            &data_y[0], &data_y[100], std::reverse_iterator<double*>(&data_z[100]));

        // push this data intp the graph object
        plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
        //
//        update_graph_limits();
    }

    // this function is no longer needed, but kept for future use
    void accept_json_ledger_buy(std::string_view data)
    {
        return;

        nlohmann::json jdata = json::parse(data);
        auto joffers = jdata["result"]["offers"];
        DEBUG_ONLY(joffers.dump(4) << std::endl);
        //
    //    app_settings* app_ini = global_settings();
        std::cout << "\n\nXRP Buy orders \n\n";
        //
        int index = 0;
        auto offers = joffers.get<std::vector<xrpl_offer>>();
        for (auto const &o : offers) {
            double xrp = o.TakerPays.value*1E-6;
            double usd = o.TakerGets.value;
            double conv = usd / xrp;
            std::cout << "rate : " << std::setw(10) << std::setprecision(7) << conv << "\t"
                      << o.Account << "\t"
                      << std::setw(10) << std::setprecision(11) << xrp << " "
                      << "$" << std::setw(10) << std::setprecision(11) << usd << std::endl;
            //
            data_x[99-index] = conv;
            data_y[99-index] = xrp;
            if (++index>=100) break;
        }
        // pad out in case there were less than 100 entries in the json
        for (; index<100; ++index) {
            data_x[99-index] = data_x[100-index];
            data_y[99-index] = data_y[100-index];
        }
        if (data_x[100]==0) {
            std::fill(&data_x[100], &data_x[200], data_x[99]);
        }
        // partial sum the 100 asks and bids, store in new y axis z array (left/bids part in reverse order)
        std::partial_sum(
            &data_y[0], &data_y[100], std::reverse_iterator<double*>(&data_z[100]));

        // push this data intp the graph object
        plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
        //
//        update_graph_limits();
    }

    void accept_json_ledger_snapshot(std::string_view data)
    {
        nlohmann::json jdata = json::parse(data);
        auto joffers = jdata["result"]["offers"];
        DEBUG_ONLY(joffers.dump(4) << std::endl);
        //
        int index = 0;
        auto offers = joffers.get<std::vector<xrpl_offer>>();
        //
        std::vector<double> bid_x, bid_y;
        std::vector<double> ask_x, ask_y;
        bid_x.reserve(600);
        bid_y.reserve(600);
        ask_x.reserve(600);
        ask_y.reserve(600);
        //
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
                bid_x.push_back(conv);
                bid_y.push_back(xrp);
            }
            // ask for $usd in return for xrp
            else if (o.TakerPays.currency==currency_type::usd_bitstamp) {
                xrp = o.TakerGets.value*1E-6;
                usd = o.TakerPays.value;
                // reject unfunded offers
                if (xrp==0 || usd==0) continue;
                //
                conv = usd / xrp;
                ask_x.push_back(conv);
                ask_y.push_back(xrp);
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

        // take the top 100 xrp buy orders and transform them to a plot format
        // partial sum the first 100 buy/bids, store in z array reverse order
        std::array<double, 200>::reverse_iterator revx(&data_x[100]);
        std::array<double, 200>::reverse_iterator revz(&data_z[100]);
        std::copy(&bid_x[0], &bid_x[100], revx);
        std::partial_sum(&bid_y[0], &bid_y[100], revz);

        // copy the ask/sell data directly
        std::copy(&ask_x[0], &ask_x[100], &data_x[100]);
        std::partial_sum(&ask_y[0], &ask_y[100], &data_z[100]);

        // push this data into the graph object
        plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
        //
//        update_graph_limits();
    }

    void ledger_map_to_plot()
    {
        std::vector<double> bid_x, bid_y;
        std::vector<double> ask_x, ask_y;
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
            const std::vector<xrpl_offer> &bids = bid_ask.first;
            const std::vector<xrpl_offer> &asks = bid_ask.second;
            //
            DEBUG_ONLY(acct
                        << " bids : " << bids.size()
                        << " asks : " << asks.size());

            for (const auto &o : bids) {
                xrp = o.TakerPays.value*1E-6;
                usd = o.TakerGets.value;
                conv = usd / xrp;
                bid_x.push_back(conv);
                bid_y.push_back(xrp);
            }
            for (const auto &o : asks) {
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
        std::array<double, 200>::reverse_iterator revx(&data_x[100]);
        std::array<double, 200>::reverse_iterator revz(&data_z[100]);
        std::copy(&bid_x[0], &bid_x[100], revx);
        std::partial_sum(&bid_y[0], &bid_y[100], revz);

        // copy the ask/sell data directly
        std::copy(&ask_x[0], &ask_x[100], &data_x[100]);
        std::partial_sum(&ask_y[0], &ask_y[100], &data_z[100]);

        //
        std::stringstream temp;
        boost::format title("%12s %12s %12s | %12s %12s %12s\n");
        temp << title % "Total" % "Size" % "Bid" % "Ask" % "Size" % "Total";
        for (int i=0; i<50; ++i) {
            boost::format num("%12.0f %12.2f %12.4f | %12.4f %12.2f %12.0f\n");
            temp << num % data_z[99-i] % bid_y[i] % bid_x[i] % ask_x[i] % ask_y[i] % data_z[100+i];
        }
        order_text = temp.str();

        // push this data into the graph object
        plot_curve_->setRawSamples(data_x.begin(), data_z.begin(), 200);
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

        // ------------------------
        // Debug
        // ------------------------
        std::string acct = transaction.at("Account").get< std::string >();
//        std::cout << "Transaction : " << transaction.dump(4) << std::endl;
//        std::cout << "Affected : " << affected.dump(4) << std::endl;
        // ------------------------

        std::string ttype = transaction.at("TransactionType").get< std::string >();
        if (ttype=="OfferCreate" || ttype=="OfferCancel" || ttype=="Payment")
        {
            try {
                handle_offer_change(transaction, affected);
            }
            catch (std::exception& e)
            {
                std::cout << "Transaction : " << transaction.dump(4) << std::endl;
                std::cout << "Affected : " << affected.dump(4) << std::endl;
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
/*
    void offer_create(const xrpl_offer &offer, const nlohmann::json &affected)
    {
        bool ok = true;
        for (auto& el : affected.items())
        {
            if (el.value().contains("CreatedNode")) {
                const auto &node = el.value()["CreatedNode"];
                std::string ltype = node["LedgerEntryType"].get< std::string >();
                if (ltype=="Offer") {
                    xrpl_offer offer2 = node["NewFields"].get< xrpl_offer >();
                    if (offer == offer2) {
                        ok = insert_offer(offer);
                        if (!ok) std::cerr << "error offer_create insert_offer" << std::endl;
                    }
                    else {
                        // this offer has been partially filled, update it
                        ok = update_offer(offer, offer2);
                        if (!ok) std::cerr << "error offer_create update_offer" << std::endl;
                    }
                }
            }
        }
        if (!ok) {
            std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
            throw std::runtime_error("Error in offer_create");
        }
    }

    void offer_cancel(const nlohmann::json &affected)
    {
        //std::cout << "offer_cancel " << affected.size() << std::endl;
        for (auto& el : affected.items())
        {
            //std::cout << el.value().dump(4) << std::endl << std::endl;
            if (el.value().contains("DeletedNode")) {
                const auto &node = el.value()["DeletedNode"];
                std::string ltype = node["LedgerEntryType"].get< std::string >();
                if (ltype=="Offer") {
                    xrpl_offer offer = node["FinalFields"].get< xrpl_offer >();
                    if (delete_offer(offer)) {
                        std::cout << "offer_cancel read and deleted" << std::endl;
                    }
                    else {
                        throw std::runtime_error("offer_cancel error");
                    }
                }
            }
        }
    }
*/
  private:
    // ----------------------------------------------------------------------------
    // bitstamp data arrives as strings instead of numbers
    // these must be converted to numeric arrays
    void bid_ask_string_to_number(nlohmann::json& json, double* x, double* y)
    {
        auto bid_string = json.get<std::array<std::array<std::string, 2>, 100>>();
        auto zip_start = boost::make_zip_iterator(boost::make_tuple(x, y));
        std::transform(bid_string.begin(), bid_string.end(), zip_start, [](const auto& i) {
            std::pair<double, double> vals =
                std::make_pair(std::atof(i[0].c_str()), std::atof(i[1].c_str()));
            return vals;
        });
    }
};
