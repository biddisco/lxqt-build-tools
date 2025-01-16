#pragma once
//
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
//
#include "nlohmann/json.hpp"
//
#include "currency/json_data_types.hpp"

constexpr static int bid_index = 0;
constexpr static int ask_index = 1;
using account_bid_ask_data =
    std::tuple<std::vector<grox::xrpl_offer>, std::vector<grox::xrpl_offer>>;
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
class order_book_base
{
  // ---------------------------------------------
  private:
  // a websocket thread might deliver bid/ask data as we are reading it, keep a lock
  mutable std::mutex bidask_mtx_;

  // ---------------------------------------------
  protected:
  using trade_set =
      std::tuple<double, double, double, double, double, double, double, double, double, double>;
  using arb_vector = std::vector<trade_set>;

  // Sorted order book entries, processed data that is derived from incoming websocket data
  offer_data bids_;
  offer_data asks_;

  // Graph min/max control
  double prev_xmin[2];
  double prev_xmax[2];
  double prev_ymax[2];

  // Text representation of order book
  std::string order_text;

  // ---------------------------------------------
  // construct
  order_book_base();

  // clean up, virtual destructor for inheritance
  virtual ~order_book_base();

  // these are not protected by a mutex and should only be accessed internally
  void update_graph_limits(bool primary);

  // produces a simple string representation of the order book from the bid/ask lists
  std::string make_order_book_string();

  // ---------------------------------------------
  public:
  std::unique_lock<std::mutex> take_bid_ask_lock() const;

  // given a max amount to spend, how much of this ask to take
  std::pair<double, double> buy_nibble(
      double max_spend, double fee_percent, double fee_fixed, double size, double rate) const;

  // given some tokens to sell, how much of this bid to take
  std::pair<double, double> sell_nibble(
      double max_tokens, double fee_percent, double fee_fixed, double size, double rate) const;

  // given another orderbook, if we buy on this one and sell on the other
  // are there arbitrage opportunities between the two
  arb_vector compute_arbitrage(order_book_base const& other, double budget, fee_data buy_fee,
      fee_data sell_fee, double test_offset, std::string& string_output) const;

  std::string get_orderbook_string() { return order_text; }

  std::tuple<double, double> get_xminmax(bool primary)
  {
    int index = primary ? 0 : 1;
    return {prev_xmin[index], prev_xmax[index]};
  }

  std::tuple<double, double> get_yminmax(bool primary)
  {
    int index = primary ? 0 : 1;
    return {0.0, prev_ymax[index]};
  }

  std::tuple<offer_data const&, offer_data const&> get_bidask_data() { return {bids_, asks_}; }
};
