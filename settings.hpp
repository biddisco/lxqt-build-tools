#pragma once

#include <QApplication>
#include <QString>

#include <string>
#include "internet/evp-encrypt.hpp"

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

    //    tempLocation = QStandardPaths::standardLocations(QStandardPaths::TempLocation).first().replace('\\', '/') + "/";
    //    desktopLocation = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first().replace('\\', '/') + "/";
    //    logFileName = QLatin1String("QtBitcoinTrader.log");
    //    iniFileName = QLatin1String("QtBitcoinTrader.ini");
};

app_settings* global_settings();
