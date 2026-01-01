// STL
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
// Qt
#include <QtCore>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
// Grox
#include "data/order_book.hpp"
#include "debug/print.hpp"
#include "exchange/order_book_bitstamp.hpp"
#include "plot/OrderBookCurve.h"
#include "plot/OrderBookPlot.h"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug::detail;
using namespace nlohmann;
template <int Level>
inline constexpr print_threshold<Level, 2> bobook_dbg("bit-book");

// ----------------------------------------------------------------------------
// Bitstamp specific order book processing routines
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// accept json reply from bitstamp order book query and turn into numeric arrays
void bitstamp_order_book::accept_json_bitstamp(QString const data)
{
  if (!startswith(data, QStringLiteral("{\"data\":"))) return;
  //
  auto l = take_bid_ask_lock();

  // convert orders into a layout we can visualize nicely
  try
  {
    std::string stdstring = data.toStdString();
    json jdata = json::parse(stdstring)["data"];
    bid_ask_string_to_number(jdata["bids"], bids_);
    bid_ask_string_to_number(jdata["asks"], asks_);
    std::partial_sum(bids_.size.begin(), bids_.size.end(), bids_.total.begin());
    std::partial_sum(asks_.size.begin(), asks_.size.end(), asks_.total.begin());
    //
    order_text = make_order_book_string();
    order_book_base::update_graph_limits(true);
  }
  catch (json::exception& e)
  {
    bobook_dbg<0>.error(ffmt<s20>("accept_json_bitstamp"), "JSON parse error:", e.what(),
        "data:", data.toStdString());
  }
}

// ----------------------------------------------------------------------------
// bitstamp data arrives as strings instead of numbers
// these must be converted to numeric arrays
void bitstamp_order_book::bid_ask_string_to_number(json& jdata, offer_data& data)
{
  bobook_dbg<5>.debug(ffmt<s20>("bid_ask_string_to_number"), jdata.size());
  //
  data.rate.resize(jdata.size(), 0);
  data.size.resize(jdata.size(), 0);
  data.total.resize(jdata.size(), 0);
  //
  std::transform(jdata.begin(), jdata.end(), ranges::view::zip(data.rate, data.size).begin(),
      [](auto const& entry)    //
      {
        std::string s1 = entry[0];
        std::string s2 = entry[1];
#ifdef GROX_ARBITRAGE_TEST_MODE
        // increase the price on the abstract_exchange to test our buy/sell algorithm
        return std::pair<double, double>{std::stod(s1) + GROX_ARBITRAGE_TEST_MODE, std::stod(s2)};
#else
      return std::pair<double, double>{ std::stod(s1), std::stod(s2) };
#endif
      });
}
