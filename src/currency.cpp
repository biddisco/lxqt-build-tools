#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
//
#include <range/v3/algorithm.hpp>
#include "currency.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
currency_type get_currency_type(issued_currency const &c)
{
    if (c.code_=="XRP") {
        return currency_type::xrp;
    }
    if (c.code_=="USD" && c.issuer_==currency::bitstamp_trust) {
        return currency_type::usd_bitstamp;
    }
    if (c.code_=="EUR" && c.issuer_==currency::bitstamp_trust) {
        return currency_type::eur_bitstamp;
    }
    if (c.code_=="USD" && c.issuer_==currency::gatehub_trust) {
        return currency_type::usd_gatehub;
    }
    for (auto const &t : currency::trustlines) {
        if ((t.issuer_==c.issuer_) && ((t.code_==c.code_) || currency_to_hex(t.code_)==c.code_))
            return currency_type::xrpl_trustline;
    }
    if (c.code_=="USD" && c.issuer_=="") {
        return currency_type::usd_unknown;
    }
    if (c.issuer_!="") {
        currency::trustlines.push_back(c);
        return currency_type::xrpl_trustline;
    }
    //
    return currency_type::other;
}

// ----------------------------------------------------------------------------
currency get_currency(std::string_view c) {
    auto icfn = [](std::string_view c) -> issued_currency {
        if (c=="USD" || c=="EUR")
            return issued_currency{currency::bitstamp_trust, std::string{c}};
        if (c=="XRP")
            return issued_currency{"", "XRP"};
        else
            return issued_currency{"", std::string{c}};
    };
    auto ic1 = icfn(c);
    return currency{ic1, get_currency_type(ic1), 0, 0, 0, nullptr};
}

// ----------------------------------------------------------------------------
bool is_fiat(currency_type c)
{
    if (c==currency_type::usd_bitstamp || c==eur_bitstamp || c==usd_gatehub)
        return true;
    return false;
}

bool is_fiat(issued_currency const &c)
{
    return is_fiat(get_currency_type(c));
}

// ----------------------------------------------------------------------------
bool is_xrp(currency_type c)
{
    if (c==currency_type::xrp)
        return true;
    return false;
}

// ----------------------------------------------------------------------------
std::pair<std::string, std::string> to_string(const currency_type &t)
{
    switch (t) {
    case xrp:
        return std::make_pair("XRP", ""); break;
    case usd_bitstamp:
        return std::make_pair("USD", currency::bitstamp_trust); break;
    case eur_bitstamp:
        return std::make_pair("EUR", currency::bitstamp_trust); break;
    case usd_gatehub:
        return std::make_pair("USD", currency::gatehub_trust); break;
    case xrpl_trustline:
        throw std::runtime_error("Insert search of trustlines here");
    case other:
        return std::make_pair("other", ""); break;
    default:
        return std::make_pair("Unknown", ""); break;
    }
    return std::make_pair("error", "");
}

// ----------------------------------------------------------------------------
std::pair<std::string, std::string> to_string(const currency &t)
{
    switch (t.type_) {
    case xrp:
        return std::make_pair("XRP", ""); break;
    case usd_bitstamp:
        return std::make_pair("USD", currency::bitstamp_trust); break;
    case eur_bitstamp:
        return std::make_pair("EUR", currency::bitstamp_trust); break;
    case usd_gatehub:
        return std::make_pair("USD", currency::gatehub_trust); break;
    case xrpl_trustline:
        return std::make_pair(t.curr_.code_, t.curr_.issuer_); break;
    case other:
        return std::make_pair("other", ""); break;
    default:
        return std::make_pair("Unknown", ""); break;
    }
    return std::make_pair("error", "");
}

// ----------------------------------------------------------------------------
std::string currency_pair_string(const currency_pair &p)
{
    std::string str = std::get<0>(p).curr_.code_ + "-" + std::get<1>(p).curr_.code_;
    return str;
}


// ----------------------------------------------------------------------------
std::string currency_pair_lowercase_string(const currency_pair &p)
{
    std::string str = std::get<0>(p).curr_.code_ + std::get<1>(p).curr_.code_;
    lowercase_i(str);
    return str;
}

// ----------------------------------------------------------------------------
currency_pair string_to_pair(std::string_view s, std::string_view delim)
{
    auto temp = s.find(delim);
    std::string_view p1 = s.substr(0, temp);
    std::string_view p2 = s.substr(temp+1, s.back());
    return {get_currency(p1), ::get_currency(p2)};
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const currency_type &t)
{
    os << to_string(t).first;
    return os;
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const currency &c)
{
    os << c.curr_.code_ << "(" << c.curr_.issuer_ << ")" << c.balance_;
    return os;
}


// ----------------------------------------------------------------------------
std::string to_string(double amount, currency_type c)
{
    int dec = 6;
    if (is_fiat(c)) {
        dec = 2;
    }
    std::stringstream stream;
    stream << std::fixed << std::setprecision(dec) << amount;
    return stream.str();
}

