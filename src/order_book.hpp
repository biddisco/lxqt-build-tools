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
#include <list>
//
#include <boost/iterator/zip_iterator.hpp>
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
    //
    std::map<std::string, std::list<double>> orders;
    //
    order_book(std::shared_ptr<OrderBookPlot> obp, bool light)
    {
        OrderBookPlot_ = obp;
        //
        std::fill(std::begin(data_x), std::end(data_x), 0);
        std::fill(std::begin(data_y), std::end(data_y), 0);
        std::fill(std::begin(data_z), std::end(data_z), 0);
        //
        prev_xmin = 0;
        prev_xmax = 0;
        //
        plot_curve_ = new OrderBookCurve();
        if (light) {
            plot_curve_->setSegmentInfo(0  , 100, QColor("green"), 2);
            plot_curve_->setSegmentInfo(100, 200, QColor("red"), 2);
            plot_curve_->setYAxis(QwtPlot::yRight);
        }
        else {
            plot_curve_->setSegmentInfo(0  , 100, QColor("darkgreen"),3);
            plot_curve_->setSegmentInfo(100, 200, QColor("darkred"),3);
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
        static bool first_time = true;
        if (first_time) {
            prev_xmin = xmin;
            prev_xmax = xmax;
            first_time = false;
        }
        else {
            double d1 = (xmin-prev_xmin)/50.0;
            double d2 = (prev_xmax-xmax)/50.0;
            prev_xmin += d1;
            prev_xmax -= d2;
        }
        //
        OrderBookPlot_->setAxisScale(QwtPlot::xBottom, prev_xmin, prev_xmax);

        // pick y min max limits so they don't jump around constantly
        double yrange = std::max(data_z[0], data_z[199]);
        double yscale = std::pow(10, static_cast<int64_t>(std::log10(yrange)));
        double ymin = 0.0;
        double ymax = (std::ceil(yrange / yscale)) * yscale;
        //
        OrderBookPlot_->setAxisScale(QwtPlot::yLeft,  ymin, ymax);
        OrderBookPlot_->setAxisScale(QwtPlot::yRight, ymin, ymax/10.0);
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
    //    app_settings* app_ini = global_settings();
        std::cout << "\n\nXRP ledger snapshot \n\n";
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
            double xrp, usd, conv;
            // bid for xrp, taker gets $usd
            if (o.TakerPays.currency==currency_type::xrp) {
                xrp = o.TakerPays.value*1E-6;
                usd = o.TakerGets.value;
                conv = usd / xrp;
                bid_x.push_back(conv);
                bid_y.push_back(xrp);
            }
            // ask for $usd in return for xrp
            else if (o.TakerPays.currency==currency_type::usd_bitstamp) {
                xrp = o.TakerGets.value*1E-6;
                usd = o.TakerPays.value;
                conv = usd / xrp;
                ask_x.push_back(conv);
                ask_y.push_back(xrp);
            }
            else {
                throw std::runtime_error("Unsupported currency");
            }

            std::cout << "rate : " << std::setw(10) << std::setprecision(7) << conv << "\t"
                      << o.Account << "\t"
                      << std::setw(12) << std::setprecision(11) << xrp << " "
                      << "$" << std::setw(12) << std::setprecision(11) << usd << std::endl;
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

    void accept_json_ledger_transaction(std::string_view data)
    {
        nlohmann::json jdata = json::parse(data);
        auto jtrans = jdata["engine_result"];
        DEBUG_ONLY(jtrans.dump() << std::endl);
        std::cout << jdata.dump(4) << std::endl;
    }

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
