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

// ----------------------------------------------------------------------------
std::string currency_pair_string(currency_pair const& p, std::string_view sep, bool add_issuer)
{
  std::string s0 = std::get<0>(p).to_stringrep(add_issuer);
  std::string s1 = std::get<1>(p).to_stringrep(add_issuer);
  return s0 + std::string(sep) + s1;
}

// ----------------------------------------------------------------------------
QString currency_pair_qstring(currency_pair const& p, std::string_view sep)
{
  return to_qstring(currency_pair_string(p, sep));
}

// ----------------------------------------------------------------------------
std::string currency_pair_lowercase_string(currency_pair const& p)
{
  std::string str = std::get<0>(p).code_ + std::get<1>(p).code_;
  lowercase_i(str);
  return str;
}

// ----------------------------------------------------------------------------
currency_pair string_to_pair(std::string_view s, std::string_view delim)
{
  auto e0 = s.find(delim);
  auto e1 = e0 + 1;
  if (delim.length() == 0)
  {
    e0 = (s.length() / 2);
    e1 = e0;
  }
  std::string_view p0 = s.substr(0, e0);
  std::string_view p1 = s.substr(e1, s.back());
  return {string_to_code(p0), string_to_code(p1)};
}

// ----------------------------------------------------------------------------
currency_pair reverse_pair(currency_pair const& cp) { return {std::get<1>(cp), std::get<0>(cp)}; }

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
