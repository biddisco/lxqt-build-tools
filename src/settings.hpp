#pragma once

#include <QApplication>
#include <QString>
//
#include <string>
//
#include "src/internet/https-async.hpp"
#include "src/internet/websocket-ssl.hpp"
#include "src/internet/evp-encrypt.hpp"
//
#include "order_book.hpp"
#include "ohlc.hpp"
#include "currency.hpp"
#include "exchange/exchange.hpp"
//

class wallet_widget;
class bitstamp_network;

struct basic_account {
    //
    std::vector<currency> currencies_;
    std::shared_ptr<exchange> network_;
};

struct ledger_wallet : public basic_account {
    secure_string  name_;
    secure_string  public_;
    secure_string  private_;
    int64_t        tag_;
    wallet_widget *widget_;
    bool           testnet_;
};

// To ensure Qt can emit signals of this type
Q_DECLARE_METATYPE(ledger_wallet)

// ----------------------------------------------------------------------------
struct bitstamp_account : public ledger_wallet {
    secure_string API_user;
    secure_string API_key;
    secure_string API_secret;
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
