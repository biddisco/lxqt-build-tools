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

currency_type get_currency_type(std::string_view name, std::string_view issuer);

// ----------------------------------------------------------------------------
struct currency {
    static inline const std::string bitstamp_trust = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    //
    std::string name_;
    std::string issuer_;
    currency_type type_;
    double balance_;
    double avail_;
    double reserved_;
    currency_widget *widget_;
};

std::ostream& operator<<(std::ostream& os, const currency_type &);
std::ostream& operator<<(std::ostream& os, const currency &);

// To ensure Qt can emit signals of this type
Q_DECLARE_METATYPE(currency)

// ----------------------------------------------------------------------------
void add_currency(const currency &curr, std::vector<currency> &c_list);
