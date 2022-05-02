#pragma once

#include <QApplication>
#include <QString>
//
#include <string>
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
//
#include "order_book.hpp"
#include "json_types.hpp"
#include "currency.hpp"
#include "src/exchange/exchange.hpp"
//

class wallet_widget;

// ----------------------------------------------------------------------------
// base class for account/wallet info
struct basic_account
{
    std::string                 name_;
    std::shared_ptr<exchange>   network_;
    wallet_widget              *widget_;
    std::vector<currency>       currencies_;
    std::vector<trade_data>     offers_;
    //
    void delete_trade(std::uint64_t id)
    {
        auto it = std::find_if(offers_.begin(), offers_.end(), [&](trade_data const& t){
            return t.id_==id;
        });
        if(it != offers_.end())
            offers_.erase(it);
    }
};

// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(basic_account*)

// ----------------------------------------------------------------------------
// an account or wallet on the xrp ledger
struct ledger_wallet : public basic_account
{
    secure_string  public_;
    secure_string  private_;
    int64_t        tag_;
    int32_t        sequence_;
    bool           testnet_;
    static inline std::mutex update_mtx_;
    //
    virtual ~ledger_wallet() {}
    virtual std::string_view get_receive_address(const currency &) { return public_; }
    void compute_ledger_reserve() {
        std::scoped_lock l(update_mtx_);
        // sort so that XRP is always first
        std::sort(currencies_.begin(), currencies_.end(),
            [](const currency &a, const currency &) -> bool {
                return (a.type_ == currency_type::xrp);
            });
        //
        int reserve = 0;
        currency *xrp = nullptr;
        for (auto &c : currencies_) {
            if (c.type_ == currency_type::xrp) xrp = &c;
            else reserve += 2;
        }
        if (xrp) {
            xrp->reserved_ = reserve;
            xrp->avail_ = xrp->balance_ - xrp->reserved_;
        }
    }
};

// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(ledger_wallet*)

// ----------------------------------------------------------------------------
// a bitstamp account (supports xrp send/receive so is also a ledger wallet)
struct bitstamp_account : public ledger_wallet {
    secure_string API_user;
    secure_string API_key;
    secure_string API_secret;
    //
    // bitstamp has a different deposit address for IOUs
    virtual std::string_view get_receive_address(const currency &c) override
    {
        if (c.type_ == currency_type::xrp) return public_;
        if (c.type_ == currency_type::usd_bitstamp) return currency::bitstamp_trust;
        if (c.type_ == currency_type::eur_bitstamp) return currency::bitstamp_trust;
        return "";
    }
};


// ----------------------------------------------------------------------------
struct app_settings
{
    QString     iniFileName;
    std::string logFileName;
    std::string hdfFileName;
    //
    std::string appDataLocation;
    std::string tempLocation;
    QString     configLocation;
    //
    std::vector<std::shared_ptr<exchange>> networks_;
    //
    secure_string grox_password;
    secure_string randomBytes;
    //
    double bitstamp_xrp_fee;
};

app_settings* global_settings();
