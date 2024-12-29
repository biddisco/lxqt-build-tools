#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include <QString>
//
#include "currency/currency.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, currency_code const& c)
{
  if (c.is_fiat()) { os << c.code_; }
  else
  {
    if (c.issuer_ == currency::bitstamp_trust)
      os << c.code_ << ".bitstamp";
    else if (c.issuer_ == currency::gatehub_trust)
      os << c.code_ << ".gatehub";
    else if (c.issuer_ == currency::ripple_trust)
      os << c.code_ << ".ripple";
    else
      os << c.code_ << "." << c.issuer_;
  }
  return os;
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, currency const& c)
{
  os << c.code_ << "(" << c.balance_ << ")";
  return os;
}

// ----------------------------------------------------------------------------
std::string to_string(double amount, currency const& c)
{
  int dec = 6;
  if (c.is_fiat()) { dec = 2; }
  std::stringstream stream;
  stream << std::fixed << std::setprecision(dec) << amount;
  return stream.str();
}

// ----------------------------------------------------------------------------
currency string_to_code(std::string_view s)
{
  auto e0 = s.find('.');
  if (e0 != std::string::npos)
  {
    auto e1 = e0 + 1;
    std::string_view p0 = s.substr(0, e0);           // code
    std::string_view p1 = s.substr(e1, s.back());    // issuer
    return {p1, p0};
  }
  return {"", s};
}

