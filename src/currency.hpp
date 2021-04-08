#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <string>
#include <vector>
#include <algorithm>

class currency_widget;

// ----------------------------------------------------------------------------
enum currency_type : int {
    xrp = 0,
    usd_bitstamp,
    eur_bitstamp,
    usd_gatehub,
    other,
};

// ----------------------------------------------------------------------------
struct currency {
    std::string name_;
    std::string issuer_;
    currency_type type_;
    double balance_;
    double avail_;
    double reserved_;
    currency_widget *widget_;
};

// To ensure Qt can emit signals of this type
Q_DECLARE_METATYPE(currency)

// ----------------------------------------------------------------------------
void add_currency(const currency &curr, std::vector<currency> &c_list);
