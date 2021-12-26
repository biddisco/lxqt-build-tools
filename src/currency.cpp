#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

#include <range/v3/algorithm.hpp>
#include "currency.hpp"

// ----------------------------------------------------------------------------
currency_type get_currency_type(std::string_view name, std::string_view issuer)
{
    if (name=="XRP") {
        return currency_type::xrp;
    }
    else if (name=="USD" && issuer=="") {
        return currency_type::usd_bitstamp;
    }
    else if (name=="USD" && issuer==currency::bitstamp_trust) {
        return currency_type::usd_bitstamp;
    }
    else if (name=="EUR" && issuer==currency::bitstamp_trust) {
        return currency_type::eur_bitstamp;
    }
    else if (name=="USD" && issuer==currency::gatehub_trust) {
        return currency_type::usd_gatehub;
    }
    else if (name=="ELS" && issuer==currency::ELS_trust) {
        return currency_type::els_trustline;
    }
    else if (issuer==currency::SOLO_trust) {
        return currency_type::solo_trustline;
    }
    else if (name=="ALV" && issuer==currency::ALV_trust) {
        return currency_type::alv_trustline;
    }
    //
    return currency_type::other;
}

// ----------------------------------------------------------------------------
bool is_fiat(currency_type c)
{
    if (c==currency_type::usd_bitstamp || c==eur_bitstamp || c==usd_gatehub)
        return true;
    return false;
}

bool is_fiat(std::string_view name, std::string_view issuer)
{
    return is_fiat(get_currency_type(name, issuer));
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
    case els_trustline:
        return std::make_pair("ELS", currency::ELS_trust); break;
    case solo_trustline:
        return std::make_pair("SOLO", currency::SOLO_trust); break;
    case alv_trustline:
        return std::make_pair("ALV", currency::ALV_trust); break;
    case other:
        return std::make_pair("other", ""); break;
    default:
        return std::make_pair("Unknown", ""); break;
    }
    return std::make_pair("error", "");
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
    os << c.name_ << " " << c.issuer_ << " " << c.balance_;
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

// ----------------------------------------------------------------------------
void add_currency(const currency &curr, std::vector<currency> &c_list)
{
    auto it = ranges::find_if(c_list, [&curr](const currency &c) {
        return c.type_ == curr.type_;
    });
    if (it==c_list.end()) {
        c_list.reserve(5);
        c_list.push_back(curr);
    }
    else {
        // copy the new currency info, but keep the old widget if it exists
        auto temp = it->widget_;
        *it = curr;
        if (temp!=nullptr) {
            it->widget_ = temp;
        }
    }
}
