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
#include "exchange/order_book.hpp"

// ----------------------------------------------------------------------------
// Bitstamp specific order book processing routines
// ----------------------------------------------------------------------------
class bitstamp_order_book : public order_book_base
{
  public:
  using order_book_base::order_book_base;

  // ----------------------------------------------------------------------------
  // accept json reply from bitstamp order book query and turn into numeric arrays
  void accept_json_bitstamp(const QString data);

  private:
  // ----------------------------------------------------------------------------
  // bitstamp data arrives as strings instead of numbers
  // these must be converted to numeric arrays
  void bid_ask_string_to_number(nlohmann::json& json, offer_data& data);
};
