// STL
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
// Qt
#include <QtCore>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/all.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
// Grox
#include "debug/print.hpp"
#include "exchange/order_book.hpp"
#include "plot/OrderBookCurve.h"
#include "plot/OrderBookPlot.h"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
using namespace grox;
using namespace grox::debug;
using namespace nlohmann;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> obook_dbg("ord-book");

// ----------------------------------------------------------------------------
// WARNING
// This ifdef increases the price on the exchange to test our buy/sell algorithm
// so that arbitrage decisions can be tested.
// ----------------------------------------------------------------------------
//#define GROX_ARBITRAGE_TEST_MODE 0.005

// ----------------------------------------------------------------------------
// Base order book class provides access to top bids_/asks_
// plotting and other representations of the orders
// ----------------------------------------------------------------------------
order_book_base::order_book_base()
  : QObject(nullptr)
{
#ifdef GROX_ARBITRAGE_TEST_MODE
  std::cerr << "**********************************************\n"
            << "Warning TEST_MODE enabled, price data invalid\n"
            << "**********************************************\n";
#endif
  for (int i = 0; i < 2; i++)
  {
    prev_xmin[i] = 0;
    prev_xmax[i] = 0;
    prev_ymax[i] = 0;
  }
}

// ----------------------------------------------------------------------------
order_book_base::~order_book_base() { obook_dbg<0>.debug(str<>("order_book_base"), "destructing"); }

// ----------------------------------------------------------------------------
orderbook_lock order_book_base::take_bid_ask_lock() const
{
  std::unique_lock<std::mutex> lock(bidask_mtx_);
  return orderbook_lock{std::move(lock)};
}

// ----------------------------------------------------------------------------
// no mutex because it should only be called when accepting new data
void order_book_base::update_graph_limits(bool primary)
{
  primary = true;

  // auto l = take_bid_ask_lock();
  // just in case multiple invocations overlap, not critical
  static std::atomic<bool> in_function = false;
  if (in_function) return;
  //
  in_function = true;
  if (asks_.rate.empty() || bids_.rate.empty()) return;
  double scale = primary ? 0.50 : 0.50;
  double tscale = primary ? 10 : 25;
  int index = primary ? 0 : 1;

  // pick x min max limits so they don't jump around constantly
  double ask_xrange = asks_.rate.back() - asks_.rate.front();
  double bid_xrange = bids_.rate.front() - bids_.rate.back();
  double min_xrange = std::min(ask_xrange, bid_xrange);
  double midpoint = (asks_.rate.front() + bids_.rate.front()) / 2.0;
  //
  double xscale = scale * std::pow(10, static_cast<int>(std::log10(min_xrange)));
  double xmin = midpoint - (min_xrange * scale);
  double xmax = midpoint + (min_xrange * scale);
  //
  // pick y min max limits so they don't jump around constantly
  double yrange = std::max(bids_.total.back(), asks_.total.back());
  double yscale = scale * std::pow(10, static_cast<int64_t>(std::log10(yrange)));
  double ymax = (std::ceil(yrange / yscale)) * yscale;
  //
  obook_dbg<6>.debug(str<>("order_book_base"), xmin, xmax, 0.0, ymax);

  static bool first_time[2] = {true, true};
  if (first_time[index])
  {
    prev_xmin[index] = xmin;
    prev_xmax[index] = xmax;
    prev_ymax[index] = ymax;
    first_time[index] = false;
  }
  else
  {
    double x1 = (xmin - prev_xmin[index]) / tscale;
    double x2 = (prev_xmax[index] - xmax) / tscale;
    double y2 = (prev_ymax[index] - ymax) / tscale;
    prev_xmin[index] += x1;
    prev_xmax[index] -= x2;
    prev_ymax[index] -= y2;
  }
  obook_dbg<6>.debug(str<>("order_book_base"), "prev_xminmax", prev_xmin[0], prev_xmax[0]);
  obook_dbg<6>.debug(str<>("order_book_base"), "prev_ymax", prev_ymax[0]);
  in_function = false;
}

// ----------------------------------------------------------------------------
// produces a simple string representation of the order book from the bid/ask lists
// no mutex because it should only be called when accepting new data
std::string order_book_base::make_order_book_string()
{
  // auto l = take_bid_ask_lock();
  // string header line
  std::stringstream temp;
  if (bids_.orig.size() == 0)
  {
    // title format string
    temp << fmt::format("{:8s} {:10s} {:10s} | {:10s} {:10s} {:8s}\n", "Total", "Size", "Bid",
        "Ask", "Size", "Total");
    // iterate over bids_/asks_
    auto zipped = ranges::views::zip(
        bids_.total, bids_.size, bids_.rate, asks_.rate, asks_.size, asks_.total);
    for (auto const& z : zipped)
    {
      temp << fmt::format("{:8.0f} {:10.2f} {:10.4f} | {:10.4f} {:10.2f} {:8.0f}\n", std::get<0>(z),
          std::get<1>(z), std::get<2>(z), std::get<3>(z), std::get<4>(z), std::get<5>(z));
    }
  }
  else
  {
    // title format string
    temp << fmt::format("{:8s} {:10s} {:10s} {:10s} | {:10s} {:10s} {:10s} {:8s}\n", "Total",
        "Size", "Orig", "Bid", "Ask", "Size", "Orig", "Total");
    // iterate over bids_/asks_
    auto zipped = ranges::views::zip(bids_.total, bids_.size, bids_.orig, bids_.rate, asks_.rate,
        asks_.size, asks_.orig, asks_.total);
    for (auto const& z : zipped)
    {
      temp << fmt::format(
          "{:8.0f} {:10.2f} {:10.2f} {:10.4f} | {:10.4f} {:10.2f} {:10.2f} {:8.0f}\n",
          std::get<0>(z), std::get<1>(z), std::get<2>(z), std::get<3>(z), std::get<4>(z),
          std::get<5>(z), std::get<6>(z), std::get<7>(z));
    }
  }
  //
  return temp.str();
}

// ----------------------------------------------------------------------------
// given a max amount to spend, how much of this ask to take
std::pair<double, double> order_book_base::buy_nibble(
    double max_spend, double fee_percent, double fee_fixed, double size, double rate) const
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

// ----------------------------------------------------------------------------
// given some tokens to sell, how much of this bid to take
std::pair<double, double> order_book_base::sell_nibble(
    double max_tokens, double fee_percent, double fee_fixed, double size, double rate) const
{
  double fee_mx = fee_percent / 100.0;
  double t_recv = std::min(size, max_tokens);
  double m_recv = t_recv * rate;
  double a_recv = (m_recv * (1.0 - fee_mx)) - fee_fixed;
  return std::make_pair(t_recv, a_recv);
}

// ----------------------------------------------------------------------------
// buy on this orderbook, sell on the other - can we earn from arbitrage
order_book_base::arb_vector order_book_base::compute_arbitrage(order_book_base const& other,
    double budget, fee_data buy_fee, fee_data sell_fee, double test_offset,
    std::string& string_output) const
{
  auto l = take_bid_ask_lock();
  //
  if (asks_.size.size() == 0 || other.bids_.size.size() == 0) return {};
  //
  auto here_ask_zipped = ranges::views::zip(asks_.size, asks_.rate);
  auto there_bid_zipped = ranges::views::zip(other.bids_.total, other.bids_.rate);
  auto sell_point = there_bid_zipped.begin();
  //
  double sell_size, sell_rate;
  std::tie(sell_size, sell_rate) = *sell_point;
  //
  QString now = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss");
  //
  // title format string
  std::stringstream temp;
  temp << now.toStdString() << "\n";
  if (test_offset > 0)
  {
    temp << "*****************************************\n"
         << fmt::format("TEST_MODE, arbitrage data offset {:6.4f} \n", test_offset)
         << "*****************************************\n";
  }
  //
  temp << fmt::format(
      "{:11s} {:11s} {:10s} {:10s} | {:11s} {:11s} {:10s} {:11s} | {:10s} {:10s} {:10s} {:10s}\n",
      "Buy", "Avail", "Price", "Cost", "Sell", "Avail", "Price", "Receive", "Gain", "%", "C_Gain",
      "C_%");
  //
  std::vector<trade_set> trades;
  //
  double spend_budget = budget;
  double cum_funds_spent = 0;
  double cum_funds_recv = 0;
  double cum_gain = 0;
  double cum_pc = 0;
  for (auto const& o : here_ask_zipped)
  {
    // if we buy the sells present in the ask list, how much do we pay?
    double ask_size, ask_rate;
    std::tie(ask_size, ask_rate) = o;
    ask_rate -= test_offset;
    //
    double tokens_bought, funds_spent;
    std::tie(tokens_bought, funds_spent) =
        buy_nibble(spend_budget, buy_fee.percent, buy_fee.fixed, ask_size, ask_rate);
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
          sell_nibble(tokens_to_sell, sell_fee.percent, sell_fee.fixed, sell_size, sell_rate);

      double gain = 0.0;
      double pc = 0.0;
      double tokens_bought_partial = tokens_bought;
      double funds_spent_partital = funds_spent;

      // if we only sold some of the tokens, compute the parital purchase cost
      if (tokens_sold < tokens_bought)
      {
        std::tie(tokens_bought_partial, funds_spent_partital) =
            buy_nibble(0, buy_fee.percent, buy_fee.fixed, tokens_sold, ask_rate);
      }
      // compute profit/loss
      gain = funds_received - funds_spent_partital;
      pc = 100.0 * (gain / funds_spent_partital);
      //
      cum_funds_recv += funds_received;
      cum_funds_spent += funds_spent_partital;
      //
      cum_gain = (cum_funds_recv - cum_funds_spent);
      cum_pc = 100.0 * (cum_gain / cum_funds_spent);
      //
      //            if (cum_pc <= 0.01) break;

      trades.push_back(trade_set{tokens_bought, ask_size, ask_rate, funds_spent_partital,
          tokens_sold, sell_size, sell_rate, funds_received, gain, pc});

      if (multi_part_sell_index > 0)
      {
        temp << fmt::format("{:11s} {:11s} {:10s} {:10s} | {:11.4f} {:11.4f} {:10.4f} {:11.4f} | "
                            "{:10.4f} {:10.4f} {:10.4f} {:10.4f}\n",
            "---", "---", "---", "---", tokens_sold, sell_size, sell_rate, funds_received, gain, pc,
            cum_gain, cum_pc);
      }
      else
      {
        temp << fmt::format("{:11.4f} {:11.4f} {:10.4f} {:10.4f} | {:11.4f} {:11.4f} {:10.4f} "
                            "{:11.4f} | {:10.4f} {:10.4f} {:10.4f} {:10.4f}\n",
            tokens_bought, ask_size, ask_rate, funds_spent_partital, tokens_sold, sell_size,
            sell_rate, funds_received, gain, pc, cum_gain, cum_pc);
      }

      tokens_to_sell -= tokens_sold;
      sell_size -= tokens_sold;
      multi_part_tokens_sold += tokens_sold;
      multi_part_funds_received += funds_received;
      multi_part_funds_spent += funds_spent_partital;

      // if all sales at this price have been made, look at the next price slot
      // (should not be < 0 - numeric precision)
      if (sell_size <= 0.0) { std::tie(sell_size, sell_rate) = *(++sell_point); }
      // end of a multi-part sale - display a summary
      if (tokens_to_sell <= 0.0 && multi_part_sell_index > 0)
      {
        gain = multi_part_funds_received - multi_part_funds_spent;
        pc = 100.0 * (gain / multi_part_funds_spent);
        //
        temp << fmt::format("{:11s} {:11s} {:10s} {:10.4f} | {:11.4f} {:11s} {:10s} {:11.4f} | "
                            "{:10.4f} {:10.4f} {:10.4f} {:10.4f}\n",
            "---", "---", "---", multi_part_funds_spent, multi_part_tokens_sold, "---", "---",
            multi_part_funds_received, gain, pc, cum_gain, cum_pc);
      }
      multi_part_sell_index++;
    }

    // always keep some small-change in the account
    if (spend_budget < 0.5) break;
  }
  // dump out the trade details
  if (trades.size() > 0)
  {
    string_output = temp.str();
    std::cout << "trades " << string_output << std::endl;
  }
  return trades;
}
