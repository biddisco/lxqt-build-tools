#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

#include <range/v3/algorithm.hpp>
#include "currency.hpp"


// ----------------------------------------------------------------------------
std::string_view to_string(const currency_type &t)
{
    switch (t) {
    case xrp:
        return "XRP"; break;
    case usd_bitstamp:
        return "USD"; break;
    case eur_bitstamp:
        return "EUR"; break;
    case usd_gatehub:
        return "USD"; break;
    case other:
        return "other"; break;
    default:
        return "Unknown"; break;
    }
    return "";
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const currency_type &t)
{
    os << to_string(t);
    return os;
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const currency &c)
{
    os << c.name_ << " " << c.issuer_ << " " << c.balance_;
    return os;
}

// ----------------------------------------------------------------------------
currency_type get_currency_type(std::string_view name, std::string_view issuer)
{
    if (name=="XRP") {
        return currency_type::xrp;
    }
    else if (name=="USD" && issuer=="") {
        return currency_type::usd_bitstamp;
    }
    else if (name=="USD" && issuer=="rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B") {
        return currency_type::usd_bitstamp;
    }
    else if (name=="EUR" && issuer=="rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B") {
        return currency_type::eur_bitstamp;
    }
    else if (name=="USD" && issuer=="rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq") {
        return currency_type::usd_gatehub;
    }
    //
    return currency_type::other;
}

bool is_fiat(currency_type c)
{
    if (c==currency_type::xrp) return false;
    return true;
}

bool is_fiat(std::string_view name, std::string_view issuer)
{
    return is_fiat(get_currency_type(name, issuer));
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
