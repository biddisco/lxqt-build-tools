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
#include "data/order_book.hpp"
#include "debug/print.hpp"
#include "exchange/order_book_xrpl.hpp"
#include "plot/OrderBookCurve.h"
#include "plot/OrderBookPlot.h"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
using namespace grox;
using namespace grox::debug::detail;
using namespace nlohmann;
template <int Level>
inline constexpr print_threshold<Level, 6> xbook_dbg("xrplbook");

// ----------------------------------------------------------------------------
// XRP ledger specific order book processing routines
// ----------------------------------------------------------------------------

// When subscribing to the ledger order book webstream
// a snapshot is included initially with the current state
// This function converts the json data into our order book form
// This function should only be executed once : when connecting to stream
void xrpl_order_book::accept_json_ledger_snapshot(nlohmann::json const& joffers)
{
  xbook_dbg<9>.debug(ffmt<s20>("snapshot"), joffers.dump(4));
  //
  // websocket (re?)connnect: clear the orderbook ...
  orders.clear();
  //
  try
  {
    auto offers = joffers.get<std::vector<xrpl_offer>>();
    //
    for (auto const& o : offers)
    {
      // skip unsupported currencies
      if (!o.grox_compatible())
      {
        std::cerr << "Unsupported currency " << o << std::endl;
        continue;
      }
      // put offer into offer map
      insert_offer(o);
    }
    //
    ledger_map_to_order_book();
  }
  catch (...)
  {
    std::cerr << "Error in accept_json_ledger_snapshot: " << joffers.dump(4) << std::endl;
  }
}

// ----------------------------------------------------------------------------
// This function converts an existing order book into plot and txt display forms
void xrpl_order_book::ledger_map_to_order_book()
{
  //
  auto l = take_bid_ask_lock();
  bids_.clear();
  asks_.clear();
  //
  auto clamp_offers_to_funds = [](std::vector<xrpl_offer>& offers, currency_code const& curr) {
    // for debugging
    //std::stringstream temp;
    //temp << acct << " : Offers : ";
    //for (auto &o : offers) { temp << o.owner_funds << ", "; }
    //std::cout << temp.str() << std::endl;

    // the first offer always holds the max funds available
    double funds_avail = offers[0].owner_funds;
    if (funds_avail == -1)
    {
      std::cerr << "Error: "
                << "bid fund tracking error " << offers[0] << std::endl;
    }
    for (auto& o : offers)
    {
      // used only in building bid/ask order books
      double amount = o.amount(curr);
      if (amount <= funds_avail) { o.funded_offer = amount; }
      else { o.funded_offer = std::max(0.0, funds_avail); }
      funds_avail -= amount;
    }
  };

  for (auto& [acct, bid_ask] : orders)
  {
    std::vector<xrpl_offer>& acc_bids_ = std::get<bid_index>(bid_ask);
    std::vector<xrpl_offer>& acc_asks_ = std::get<ask_index>(bid_ask);
    //
    xbook_dbg<7>.debug(
        ffmt<s20>("bid/ask"), acct, "bids:", acc_bids_.size(), "asks:", acc_asks_.size());

    // the account may not be fully funded, so the offers may be invalid
    if (acc_bids_.size() > 0)
    {
      std::sort(acc_bids_.begin(), acc_bids_.end(), std::greater<xrpl_offer>{});
      clamp_offers_to_funds(acc_bids_, {currency_issuers::bitstamp_trust, "USD"});
    }
    double tiny_offers = 0;
    for (auto const& o : acc_bids_)
    {
      auto xrp_amount = o.amount({"", "XRP"}) * 1E-6;
      // skip unfunded or very small offers
      if (o.unfunded(0.1))
      {
        tiny_offers += xrp_amount;
        continue;
      }
      //
      bids_.rate.push_back(o.rate());
      bids_.orig.push_back(xrp_amount + tiny_offers);
      bids_.size.push_back(o.funded_offer / o.rate());
      tiny_offers = 0;
    }

    if (acc_asks_.size() > 0)
    {
      std::sort(acc_asks_.begin(), acc_asks_.end(), std::less<xrpl_offer>{});
      clamp_offers_to_funds(acc_asks_, {"", "XRP"});
    }
    tiny_offers = 0;
    for (auto const& o : acc_asks_)
    {
      auto xrp_amount = o.amount({"", "XRP"}) * 1E-6;
      // skip unfunded or very small offers
      if (o.unfunded(0.1))
      {
        tiny_offers += xrp_amount;
        continue;
      }
      // skip unfunded or very small offers
      if (o.unfunded(0.1 * 1E6)) continue;
      //
      asks_.rate.push_back(o.rate());
      asks_.orig.push_back(xrp_amount + tiny_offers);
      asks_.size.push_back(o.funded_offer * 1E-6);
      tiny_offers = 0;
    }
  }

  // sort zipped X/Y bids_ from high to low, sort based on rate
  ranges::sort(ranges::views::zip(bids_.rate, bids_.size, bids_.orig),
      [](auto&& a, auto&& b) { return std::get<0>(a) > std::get<0>(b); });

  // sort zipped X/Y asks_ from low to high, sort based on X=conv
  ranges::sort(ranges::views::zip(asks_.rate, asks_.size, asks_.orig),
      [](auto&& a, auto&& b) { return std::get<0>(a) < std::get<0>(b); });

  // partial sum the bids_
  bids_.total.resize(bids_.size.size());
  std::partial_sum(bids_.size.begin(), bids_.size.end(), bids_.total.begin());

  // partial sum the asks_
  asks_.total.resize(asks_.size.size());
  std::partial_sum(asks_.size.begin(), asks_.size.end(), asks_.total.begin());

  // push this data into the graph object
  order_book_base::update_graph_limits(true);
  order_text = make_order_book_string();
}

// ----------------------------------------------------------------------------
void xrpl_order_book::accept_json_ledger_transaction(nlohmann::json const& jdata)
{
  std::string success = jdata.at("engine_result").get<std::string>();
  if (success != "tesSUCCESS") return;
  //
  json affected = jdata["meta"]["AffectedNodes"];
  xbook_dbg<7>.debug(ffmt<s20>("Affected nodes"), affected.dump(4));

  json transaction = jdata["transaction"];
  xbook_dbg<7>.debug(ffmt<s20>("transaction"), transaction.dump(4));

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

// ----------------------------------------------------------------------------
bool xrpl_order_book::update_offer(
    xrpl_offer const& prev_offer, xrpl_offer& final_offer, double owner_funds)
{
  std::string const& acct = prev_offer.Account;
  offer_map::iterator it = orders.find(acct);
  // if not in map
  if (it == orders.end())
  {
    std::cerr << prev_offer.Account << " update_offer address not in map" << std::endl;
    return false;
  }
  //
  std::vector<xrpl_offer>& acc_bids_ = std::get<bid_index>(it->second);
  std::vector<xrpl_offer>& acc_asks_ = std::get<ask_index>(it->second);
  if (prev_offer.TakerPays.currency_.is_xrp())
  {
    auto it2 = std::find(acc_bids_.begin(), acc_bids_.end(), prev_offer);
    if (it2 == acc_bids_.end())
    {
      std::cerr << prev_offer.Account << " update_offer bid not found" << std::endl;
      return false;
    }
    // to enable fund tracking
    if (it2->owner_funds != -1 && final_offer.owner_funds == -1)
    {
      final_offer.owner_funds = it2->owner_funds;
    }
    // overwrite old offer with new one
    xbook_dbg<5>.debug(ffmt<s20>("Update Bid:"), prev_offer, final_offer);
    *it2 = final_offer;
  }
  else
  {
    auto it2 = std::find(acc_asks_.begin(), acc_asks_.end(), prev_offer);
    if (it2 == acc_asks_.end())
    {
      std::cerr << prev_offer.Account << " update_offer ask not found" << std::endl;
      return false;
    }
    // overwrite old offer with new one
    xbook_dbg<5>.debug(ffmt<s20>("Update Ask:"), prev_offer, final_offer);
    *it2 = final_offer;
  }
  return true;
}

// ----------------------------------------------------------------------------
bool xrpl_order_book::insert_offer(xrpl_offer const& offer)
{
  std::string const& acct = offer.Account;
  offer_map::iterator it = orders.find(acct);
  // if not in map, create new entry
  if (it == orders.end())
  {
    account_bid_ask_data bid_ask{{}, {}};
    auto const [it2, success] = orders.insert({offer.Account, bid_ask});
    if (success)
      it = it2;
    else
    {
      std::cerr << offer.Account << " insert_offer map insert error" << std::endl;
      return false;
    }
  }
  // add new order to map vectors
  std::vector<xrpl_offer>& acc_bids_ = std::get<bid_index>(it->second);
  std::vector<xrpl_offer>& acc_asks_ = std::get<ask_index>(it->second);
  if (offer.TakerPays.currency_.is_xrp())
  {
    acc_bids_.push_back(offer);
    xbook_dbg<7>.debug(ffmt<s20>("Insert Bid:"), offer);
  }
  else
  {
    acc_asks_.push_back(offer);
    xbook_dbg<7>.debug(ffmt<s20>("Insert Ask:"), offer);
  }
  return true;
}

// ----------------------------------------------------------------------------
bool xrpl_order_book::delete_offer(xrpl_offer const& offer)
{
  std::string const& acct = offer.Account;
  offer_map::iterator it = orders.find(acct);
  // if not in map
  if (it == orders.end())
  {
    std::cerr << offer.Account << " delete_offer address not in map" << std::endl;
    return false;
  }
  // remove order from map vector
  std::vector<xrpl_offer>& acc_bids_ = std::get<bid_index>(it->second);
  std::vector<xrpl_offer>& acc_asks_ = std::get<ask_index>(it->second);
  if (offer.TakerPays.currency_.is_xrp())
  {
    auto val = std::find(acc_bids_.begin(), acc_bids_.end(), offer);
    if (val == acc_bids_.end())
    {
      std::cerr << "Error : Bid delete not found" << std::endl;
      std::cerr << "Bid: " << offer << std::endl;
      return false;
    }
    else if (val != std::prev(acc_bids_.end()))
    {
      // update tracking of account funds
      if (val->owner_funds != -1)
      {
        xbook_dbg<5>.debug(ffmt<s20>("Update owner_funds"), val->owner_funds);
        std::next(val)->owner_funds = val->owner_funds;
      }
    }
    xbook_dbg<7>.debug(ffmt<s20>("Delete Bid:"), offer);
    acc_bids_.erase(val);
  }
  else
  {
    auto val = std::find(acc_asks_.begin(), acc_asks_.end(), offer);
    if (val == acc_asks_.end())
    {
      std::cerr << "Ask: " << offer << std::endl;
      std::cerr << "Error : Ask delete not found" << std::endl;
      return false;
    }
    else if (val != std::prev(acc_asks_.end()))
    {
      // update tracking of account funds
      if (val->owner_funds != -1)
      {
        xbook_dbg<5>.debug(ffmt<s20>("Update owner_funds"), val->owner_funds);
        std::next(val)->owner_funds = val->owner_funds;
      }
    }
    xbook_dbg<7>.debug(ffmt<s20>("Delete Ask:"), offer);
    acc_asks_.erase(val);
  }
  if (acc_bids_.size() == 0 && acc_asks_.size() == 0)
  {
    // we can safely remove the account
    xbook_dbg<5>.debug(ffmt<s20>("Account"), offer.Account, "can be removed");
    orders.erase(offer.Account);
  }
  return true;
}

// ----------------------------------------------------------------------------
enum node_edit
{
  created = 0,
  modified,
  deleted
};

// ----------------------------------------------------------------------------
void xrpl_order_book::handle_offer_change(json const& trans, json const& affected)
{
  bool ok = true;
  bool fatal = true;
  for (auto& el : affected.items())
  {
    json const* node;
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
      if (!final_offer.grox_compatible()) continue;
    }
    if (node->contains("FinalFields"))
    {
      final_offer = (*node)["FinalFields"].get<xrpl_offer>();
      if (!final_offer.grox_compatible()) continue;
    }
    if (node->contains("PreviousFields"))
    {
      prev_offer = (*node)["FinalFields"].get<xrpl_offer>();
      if (!prev_offer.grox_compatible()) continue;
    }

    // To track unfunded offers, we add the "owner_funds" as it will
    // appear in the transaction, but not the "offer" node,
    // (except when receeiving initial order book)
    double owner_funds = -1;
    if (trans.contains("owner_funds"))
    {
      owner_funds = std::stod(trans.at("owner_funds").get<std::string>());
      final_offer.owner_funds = owner_funds;
    }

    switch (edit_type)
    {
    case node_edit::created: ok &= insert_offer(final_offer); break;
    case node_edit::modified: ok &= update_offer(prev_offer, final_offer); break;
    case node_edit::deleted:
      ok &= delete_offer(final_offer);
      // if an offer delete fails, it's not fatal
      fatal = false;
      break;
    }
  }
  if (!ok)
  {
    std::cerr << "Error : Transaction : " << trans.dump(4) << std::endl;
    std::cerr << "Error : Affected : " << affected.dump(4) << std::endl;
    if (fatal) throw std::runtime_error("Error in handle_offer_change");
  }
}
