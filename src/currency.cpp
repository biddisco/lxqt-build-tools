#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

#include <range/v3/algorithm.hpp>
#include "currency.hpp"

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const currency_type &t)
{
    switch (t) {
    case xrp:
        os << "XRP"; break;
    case usd_bitstamp:
        os << "USD"; break;
    case eur_bitstamp:
        os << "EUR"; break;
    case usd_gatehub:
        os << "USD"; break;
    case other:
        os << "other"; break;
    default:
        os << "Unknown"; break;
    }
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
