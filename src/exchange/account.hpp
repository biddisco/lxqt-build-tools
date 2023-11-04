#pragma once

#include <QApplication>
#include <QString>
#include <QTimer>
//
#include <string>
//
#include "config/config.hpp"
//
#include "network/evp-encrypt.hpp"
#include "network/https-async.hpp"
#include "network/websocket-ssl.hpp"
//
#include "currency/currency.hpp"
#include "data/ohlc_dataset_manager.hpp"
#include "exchange/exchange.hpp"
#include "exchange/order_book.hpp"
//
class wallet_widget;

// ----------------------------------------------------------------------------
// base class for account/wallet info
struct basic_account
{
  using lock_type = std::unique_lock<std::mutex>;
  //
  std::string name_;
  std::shared_ptr<exchange> network_;
  wallet_widget* widget_;
  std::vector<currency> currencies_;
  std::vector<trade_data> offers_;
  static inline std::mutex currency_mtx_;
  //
  void delete_trade(std::uint64_t id)
  {
    auto it = std::find_if(
      offers_.begin(), offers_.end(), [&](trade_data const& t) { return t.id_ == id; });
    if (it != offers_.end())
      offers_.erase(it);
  }

  // ----------------------------------------------------------------------------
  void add_currency(currency const& curr)
  {
    std::scoped_lock l(currency_mtx_);
    //
    auto it = ranges::find_if(currencies_, [&curr](currency const& c) {
      return c.type_ == curr.type_ && c.curr_.code_ == curr.curr_.code_ &&
        c.curr_.issuer_ == curr.curr_.issuer_;
    });
    if (it == currencies_.end())
    {
      currencies_.push_back(curr);
    }
    else
    {
      // copy the new currency info, but keep the old widget if it exists
      auto temp = it->widget_;
      *it = curr;
      if (temp != nullptr)
      {
        it->widget_ = temp;
      }
    }
  }

  lock_type lock_currencies()
  {
    lock_type l(currency_mtx_);
    return std::move(l);
  }
  //
  void unlock_currencies(lock_type&& l)
  {
    l.unlock();
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
  virtual std::string_view get_receive_address(currency const&)
  {
    return public_;
  }
  //
  void compute_ledger_reserve()
  {
    std::scoped_lock l(currency_mtx_);
    // sort so that XRP is always first
    std::sort(currencies_.begin(), currencies_.end(),
      [](currency const& a, currency const&) -> bool { return (a.type_ == currency_type::xrp); });
    //
    int reserve = 0;
    currency* xrp = nullptr;
    for (auto& c : currencies_)
    {
      if (c.type_ == currency_type::xrp)
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
  virtual std::string_view get_receive_address(currency const& c) override
  {
    if (c.type_ == currency_type::xrp)
      return public_;
    if (c.type_ == currency_type::usd_bitstamp)
      return currency::bitstamp_trust;
    if (c.type_ == currency_type::eur_bitstamp)
      return currency::bitstamp_trust;
    return "";
  }
};
