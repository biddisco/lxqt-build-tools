#include <iostream>
#include <string>
#include <vector>
//
#include "nlohmann/json.hpp"
//
#include "currency/currency.hpp"
#include "currency/json_data_types.hpp"

namespace grox {
  using namespace nlohmann;

  std::ostream& operator<<(std::ostream& os, xrp_amount const& x)
  {
    os << x.currency_;
    return os;
  }

  std::ostream& operator<<(std::ostream& os, xrpl_offer const& x)
  {
    os << "Account: " << x.Account << " "
       << "BookDirectory: " << x.BookDirectory << " "
       << "TakerPays: " << x.TakerPays << " "
       << "TakerGets: " << x.TakerGets << " "
       << "Rate: " << x.rate() << " "
       << "Funds: " << x.owner_funds;
    return os;
  }

  // ----------------------------------------------------------------------------
  // ohlc data
  // ----------------------------------------------------------------------------
  /* New style using RPC json API
    "account": "rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY",
    "balance": "63.8354397",
    "currency": "5041534100000000000000000000000000000000",
    "limit": "100000000",
    "limit_peer": "0",
    "no_ripple": true,
    "no_ripple_peer": false,
    "quality_in": 0,
    "quality_out": 0
*/
  /* Old style using v2 data API
    "currency": "5041534100000000000000000000000000000000",
    "counterparty": "rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY",
    "value": "63.8354397"
*/

  void from_json(json const& j, xrp_amount& p)
  {
    // if this is a simple value (just plain XRP amount)
    if (j.size() == 1)
    {
      p.currency_ = {"", "XRP"};
      p.value_ = std::stod(j.get<std::string>());
    }
    else
    {
      // allow balance OR value string id
      if (j.count("balance") != 0) { p.value_ = std::stod(j.at("balance").get<std::string>()); }
      else if (j.count("value") != 0) { p.value_ = std::stod(j.at("value").get<std::string>()); }
      else
        throw std::runtime_error("No value in currency amount");
      //
      std::string currency = j.at("currency").get<std::string>();
      //
      std::string issuer;
      // allow account/issuer/counterparty string id
      if (j.count("account") != 0) { issuer = j.at("account").get<std::string>(); }
      else if (j.count("counterparty") != 0) { issuer = j.at("counterparty").get<std::string>(); }
      else if (j.count("issuer") != 0) { issuer = j.at("issuer").get<std::string>(); }
      //
      p.currency_ = {issuer, currency};
    }
  }

  // ----------------------------------------------------------------------------
  void from_json(json const& j, xrpl_offer& p)
  {
    bool ok = true;
    p.funded_offer = -1;
    p.Account = j.at("Account").get<std::string>();
    p.BookDirectory = j.at("BookDirectory").get<std::string>();
    p.TakerGets = j.at("TakerGets").get<xrp_amount>();
    p.TakerPays = j.at("TakerPays").get<xrp_amount>();

    // we track unfunded offers using owner funds - this code is obsolete
#ifdef OLD_TAKER_PAYS_FUNDED
    if (j.contains("taker_gets_funded"))
    {
      xrpl_offer temp = p;
      temp.TakerGets = j.at("taker_gets_funded").get<xrp_amount>();
      temp.TakerPays = j.at("taker_pays_funded").get<xrp_amount>();
      if (temp.TakerGets.value > 0 && temp.TakerPays.value > 0) { p = temp; }
    }
#endif
    // The offer might be for more than the account actually holds
    // so we must track actual funds when building order book later
    if (j.contains("owner_funds"))
    {
      p.owner_funds = std::stod(j.at("owner_funds").get<std::string>());
    }
    else { p.owner_funds = -1; }
  }

  // ----------------------------------------------------------------------------
  void from_json(json const& j, price& p)
  {
    if (j.size() == 2)
    {
      if (j[0] == nullptr)
        p.value = 0;
      else
        p.value = stod(j[0].get<std::string>());
      p.time = j[1];
    }
  }
}    // namespace grox
