#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include <QString>
//
#include "currency/currency.hpp"

struct currency_pair
{
  using list = std::vector<currency_pair>;
  //
  currency_code c1_;
  currency_code c2_;
  //
  bool operator==(currency_pair const& c) const { return (c1_ == c.c1_) && (c2_ == c.c2_); }
  bool operator<(currency_pair const& c) const
  {
    return (c1_ < c.c1_) || ((c1_ == c.c1_) && (c2_ < c.c2_));
  }
  bool operator>(currency_pair const& c) const
  {
    return (c1_ > c.c1_) || ((c1_ == c.c1_) && (c2_ > c.c2_));
  }

  // stream operators
  friend std::ostream& operator<<(std::ostream&, currency_pair const&);
};

// ----------------------------------------------------------------------------
currency_pair get_currency_pair(std::string_view c1, std::string_view c2);
currency_pair string_to_pair(std::string_view s, std::string_view delim);
currency_pair reverse_pair(currency_pair const& cp);

// ----------------------------------------------------------------------------
std::string currency_pair_string(
    currency_pair const& p, std::string_view sep = "-", bool add_issuer = true);
QString currency_pair_qstring(
    currency_pair const& p, std::string_view sep = "-", bool add_issuer = true);
std::string currency_pair_lowercase_string(currency_pair const& p);
