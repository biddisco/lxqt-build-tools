#pragma once

#include <QApplication>
#include <QString>

#include <string>
#include "src/internet/evp-encrypt.hpp"

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
    secure_string API_user;
    secure_string API_key;
    secure_string API_secret;
    //
    secure_string XRP_name;
    secure_string XRP_public;
    secure_string XRP_secret;
    //
    secure_string grox_password;
    secure_string randomBytes;
    //
    double bitstamp_xrp_fee;
    //
    double bitstamp_xrp_balance;
    double bitstamp_xrp_available;
    double bitstamp_xrp_reserved;
    //
    double bitstamp_usd_balance;
    double bitstamp_usd_available;
    double bitstamp_usd_reserved;
    //
    double ledger_xrp_balance;
    double ledger_xrp_available;
    double ledger_xrp_reserved;
    //
    double ledger_usd_balance;
    double ledger_usd_available;
    double ledger_usd_reserved;
    //
    //    tempLocation = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().replace('\\', '/') + "/";
    //    desktopLocation = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first().replace('\\', '/') + "/";
    //    logFileName = QLatin1String("QtBitcoinTrader.log");
    //    iniFileName = QLatin1String("QtBitcoinTrader.ini");
};

app_settings* global_settings();
