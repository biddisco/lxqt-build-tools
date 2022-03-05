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
std::string currency_to_hex(std::string_view name);
std::string hex_to_currency(std::string_view name);

// ----------------------------------------------------------------------------
enum currency_type : int {
    xrp = 0,
    usd_bitstamp,
    eur_bitstamp,
    usd_gatehub,
    xrpl_trustline,
    other,
};

// ----------------------------------------------------------------------------
struct issued_currency {
    std::string issuer_;
    std::string code_;
};

// ----------------------------------------------------------------------------
struct currency {
    static inline const std::string bitstamp_trust = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    static inline const std::string gatehub_trust = "rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq";
    //
    static inline std::vector<issued_currency> trustlines = {
//        {"rHXuEaRYnnJHbDeuBH5w8yPh5uwNVh5zAg", "ELS"},
//        {"rsoLo2S1kiGeCcn6hCUXVrCpGMWLrRrLZz", "SOLO"},
//        {"raEQc5krJ2rUXyi6fgmUAf63oAXmF7p6jp", "ALV"},
//        {"rM7zpZQBfz9y2jEkDrKcXiYPitJx9YTS1J", "DKP"},
//        {"rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY", "1"},
//        {"rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY", "PASA"},
    };
    //
    issued_currency curr_;
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
currency_type get_currency_type(issued_currency const &c);

// ----------------------------------------------------------------------------
// return true if the currency is a fiat currency such as USD, EUR etc etc
bool is_fiat(currency_type c);
bool is_fiat(issued_currency const &c);
bool is_xrp(currency_type c);

// ----------------------------------------------------------------------------
// convert a currency type enum to a string pair, {name, issuer}
std::pair<std::string, std::string> to_string(const currency_type &t);
std::pair<std::string, std::string> to_string(const currency &t);

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
