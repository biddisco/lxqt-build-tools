#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//
#include <range/v3/algorithm.hpp>
//
#include <QApplication>
#include <QString>
#include <QTimer>
//
#include "config/config.hpp"
#include "currency/currency.hpp"
#include "data/order_book.hpp"
#include "exchange/abstract_exchange.hpp"
#include "network/evp-encrypt.hpp"
//
class wallet_widget;

// ----------------------------------------------------------------------------
std::string currency_to_hex(std::string_view name);
std::string hex_to_currency(std::string_view name);

// ----------------------------------------------------------------------------
// base class for account/wallet info
struct basic_account
{
  using lock_type = std::unique_lock<std::mutex>;
  //
  std::string name_;
  std::shared_ptr<abstract_exchange> network_;
  wallet_widget* widget_;
  std::vector<currency_amount> currencies_;
  std::vector<trade_data> offers_;
  static inline std::mutex currency_mtx_;
  static inline std::mutex protection_;
  //
  void delete_trade(std::uint64_t id)
  {
    auto it = std::find_if(
        offers_.begin(), offers_.end(), [&](trade_data const& t) { return t.id_ == id; });
    if (it != offers_.end()) offers_.erase(it);
  }

  // ----------------------------------------------------------------------------
  void add_currency(currency_amount const& curr)
  {
    std::scoped_lock l(currency_mtx_);
    //
    auto it =
        ranges::find_if(currencies_, [&curr](currency_amount const& c) { return (c == curr); });
    if (it == currencies_.end()) { currencies_.push_back(curr); }
    else
    {
      // copy the new currency info, but keep the old widget if it exists
      auto temp = it->widget_;
      *it = curr;
      if (temp != nullptr) { it->widget_ = temp; }
    }
  }

  lock_type lock_currencies()
  {
    lock_type l(currency_mtx_);
    return std::move(l);
  }
  //
  void unlock_currencies(lock_type&& l) { l.unlock(); }

  //
  void add_trade(trade_data&& t, bool confirmed)
  {
    std::scoped_lock l(protection_);
    // if the trade is already in our list, then just update confirmation status
    for (auto& o : offers_)
    {
      if (o.id_ == t.id_)
      {
        o.confirmed_ = confirmed;
        return;
      }
    }
    offers_.push_back(std::move(t));
  }

  //
  void remove_trade(trade_data const& t)
  {
    std::scoped_lock l(protection_);
    // remove if trade id matches passed in value
    std::remove_if(offers_.begin(), offers_.end(), [&t](auto const& o) { return o.id_ == t.id_; });
  }
};

// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(basic_account*)

// ----------------------------------------------------------------------------
// an account or wallet on the xrp ledger
struct ledger_wallet : public basic_account
{
  secure_string public_;
  secure_string private_;
  int64_t tag_;
  int32_t sequence_;
  bool testnet_;
  //
  virtual ~ledger_wallet() {}
  virtual std::string_view get_receive_address(currency_code const&) { return public_; }
  //
  void compute_ledger_reserve()
  {
    std::scoped_lock l(currency_mtx_);
    // sort so that XRP is always first
    std::sort(currencies_.begin(), currencies_.end(),
        [](currency_amount const& a, currency_amount const&) -> bool {
          return (a.symbol_.is_xrp());
        });
    //
    int reserve = 0;
    currency_amount* xrp = nullptr;
    for (auto& c : currencies_)
    {
      if (c.symbol_.is_xrp())
        xrp = &c;
      else
        reserve += 2;
    }
    if (xrp)
    {
      xrp->reserved_ = reserve;
      xrp->avail_ = xrp->balance_ - xrp->reserved_;
    }
  }
};

// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(ledger_wallet*)

// ----------------------------------------------------------------------------
// a bitstamp account (supports xrp send/receive so is also a ledger wallet)
struct bitstamp_account : public ledger_wallet
{
  secure_string API_user;
  secure_string API_key;
  secure_string API_secret;
  //
  // bitstamp has a different deposit address for IOUs
  virtual std::string_view get_receive_address(currency_code const& c) override
  {
    if (c.is_xrp()) return public_;
    if (c == currency_code{currencies::bitstamp_trust, "USD"}) return currencies::bitstamp_trust;
    if (c == currency_code{currencies::bitstamp_trust, "EUR"}) return currencies::bitstamp_trust;
    return "";
  }
};
