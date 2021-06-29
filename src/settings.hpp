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
#include "ohlc.hpp"
#include "currency.hpp"
#include "src/exchange/exchange.hpp"
//

class wallet_widget;
class bitstamp_network;

struct basic_account {
    //
    std::vector<currency> currencies_;
    std::shared_ptr<exchange> network_;
};
// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(basic_account*)

struct ledger_wallet : public basic_account {
    secure_string  name_;
    secure_string  public_;
    secure_string  private_;
    int64_t        tag_;
    int32_t        sequence_;
    wallet_widget *widget_;
    bool           testnet_;

    virtual ~ledger_wallet() {}
    virtual std::string_view get_receive_address(const currency &c) { return public_; }
};

// For compatibility with Qt Variant and Signals/Slots
Q_DECLARE_METATYPE(ledger_wallet*)

// ----------------------------------------------------------------------------
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
    }
};


// ----------------------------------------------------------------------------
struct app_settings
{
    QString iniFileName;
    std::string logFileName;
    std::string hdfFileName;
    //
    std::string appDataLocation;
    std::string tempLocation;
    QString configLocation;
    //
    bitstamp_account bitstamp;
    //
    std::vector<ledger_wallet> xrpl_wallets;
    //
    secure_string grox_password;
    secure_string randomBytes;
    //
    double bitstamp_xrp_fee;
};

app_settings* global_settings();
