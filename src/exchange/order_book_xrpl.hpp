#pragma once
//
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
//
#include <QtCore>
//
#include "nlohmann/json.hpp"
//
#include "currency/json_data_types.hpp"
#include "exchange/order_book_xrpl.hpp"

// ----------------------------------------------------------------------------
// XRP ledger specific order book processing routines
// ----------------------------------------------------------------------------
class xrpl_order_book : public order_book_base
{
  public:
  using order_book_base::order_book_base;
  //
  offer_map orders;

  // When subscribing to the ledger order book webstream
  // a snapshot is inculded initially with the current state
  // This function converts the json into our order book
  void accept_json_ledger_snapshot(nlohmann::json joffers);

  void ledger_map_to_order_book();

  void accept_json_ledger_transaction(nlohmann::json jdata);

  bool update_offer(
    grox::xrpl_offer const& prev_offer, grox::xrpl_offer& final_offer, double owner_funds = -1);

  bool insert_offer(grox::xrpl_offer const& offer);

  bool delete_offer(grox::xrpl_offer const& offer);

  enum node_edit
  {
    created = 0,
    modified,
    deleted
  };

  void handle_offer_change(nlohmann::json const& trans, nlohmann::json const& affected);
};
