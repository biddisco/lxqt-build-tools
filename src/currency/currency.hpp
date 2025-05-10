#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include "currency/currency_code.hpp"

// ----------------------------------------------------------------------------
struct currency_amount
{
  currency_amount(currency_code curr, double balance, double avail = 0.0, double reserved = 0.0)
    : symbol_(curr)
    , balance_(balance)
    , avail_(avail)
    , reserved_(reserved)
  {
  }

  // comparison operators
  bool operator==(currency_amount const& c) const { return (symbol_ == c.symbol_); }
  bool operator<(currency_amount const& c) const { return symbol_ < c.symbol_; }
  bool operator>(currency_amount const& c) const { return symbol_ > c.symbol_; }

  // stream operators
  friend std::ostream& operator<<(std::ostream&, currency_amount const&);

  currency_code symbol_;
  double balance_;
  double avail_;
  double reserved_;
};

// ----------------------------------------------------------------------------
template <typename T>
std::string to_string_with_precision(T const a_value, int const n = 6)
{
  std::ostringstream out;
  out.precision(n);
  out << std::fixed << a_value;
  return out.str();
}
