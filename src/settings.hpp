#pragma once

#include <QApplication>
#include <QString>

#include <string>
#include "src/internet/evp-encrypt.hpp"
#include "ohlc.hpp"

class wallet_widget;
class currency_widget;

struct currency {
    std::string name_;
    std::string issuer_;
    currency_type type_;
    double balance_;
    double avail_;
    double reserved_;
    currency_widget *widget_;
};

struct basic_account {
    //
    std::vector<currency> currencies_;
};

struct ledger_wallet : public basic_account {
    secure_string  name_;
    secure_string  public_;
    secure_string  private_;
    int64_t        tag_;
    wallet_widget *widget_;
};

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
    int active_wallet;
    std::vector<ledger_wallet> xrp_wallets;
    //
    secure_string grox_password;
    secure_string randomBytes;
    //
    double bitstamp_xrp_fee;
    //
    //    tempLocation = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().replace('\\', '/') + "/";
    //    desktopLocation = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first().replace('\\', '/') + "/";
    //    logFileName = QLatin1String("QtBitcoinTrader.log");
    //    iniFileName = QLatin1String("QtBitcoinTrader.ini");
};

app_settings* global_settings();
