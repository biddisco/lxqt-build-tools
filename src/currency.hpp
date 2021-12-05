#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

class currency_widget;

// ----------------------------------------------------------------------------
enum currency_type : int {
    xrp = 0,
    usd_bitstamp,
    eur_bitstamp,
    usd_gatehub,
    els_trustline,
    other,
};

// ----------------------------------------------------------------------------
struct currency {
    static inline const std::string bitstamp_trust = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    static inline const std::string gatehub_trust = "rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq";
    static inline const std::string ELS_trust = "rHXuEaRYnnJHbDeuBH5w8yPh5uwNVh5zAg";
    //
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
// convert a string pair, name, issuer to a currency type enum
currency_type get_currency_type(std::string_view name, std::string_view issuer);

// ----------------------------------------------------------------------------
// return true if the currency is a fiat currency such as USD, EUR etc etc
bool is_fiat(currency_type c);
bool is_fiat(std::string_view name, std::string_view issuer);

// ----------------------------------------------------------------------------
// convert a currency type enum to a string pair, {name, issuer}
std::pair<std::string, std::string> to_string(const currency_type &t);

// ----------------------------------------------------------------------------
// displays an amount such as 1.34 as a string, but uses different numbers
// of decimal places depending on the currency type (fiat always 2)
std::string to_string(double amount, currency_type c);

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

// ----------------------------------------------------------------------------
// stream operators
std::ostream& operator<<(std::ostream& os, const currency_type &);
std::ostream& operator<<(std::ostream& os, const currency &);

// ----------------------------------------------------------------------------
// convenience function to add a currency to a list
void add_currency(const currency &curr, std::vector<currency> &c_list);
