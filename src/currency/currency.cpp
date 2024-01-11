#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include "currency/currency.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
currency::currency(const issued_currency& c)
{
  curr_ = c;
  balance_ = 0.0;
  avail_ = 0.0;
  reserved_ = 0.0;
  widget_ = nullptr;
}

// ----------------------------------------------------------------------------
currency::currency(
  issued_currency curr, double balance, double avail, double reserved, currency_widget* widget)
  : curr_(curr)
  , balance_(balance)
  , avail_(avail)
  , reserved_(reserved)
  , widget_(widget)
{
}

// ----------------------------------------------------------------------------
/*
currency_type get_currency_type(issued_currency const& c)
{
  if (c.code_ == "XRP")
  {
    return currency_type::xrp;
  }
  if (c.code_ == "USD" && c.issuer_ == currency::bitstamp_trust)
  {
    return currency_type::usd_bitstamp;
  }
  if (c.code_ == "EUR" && c.issuer_ == currency::bitstamp_trust)
  {
    return currency_type::eur_bitstamp;
  }
  if (c.code_ == "USD" && c.issuer_ == currency::gatehub_trust)
  {
    return currency_type::usd_gatehub;
  }
  for (auto const& t : currency::trustlines)
  {
    if ((t.issuer_ == c.issuer_) && ((t.code_ == c.code_) || currency_to_hex(t.code_) == c.code_))
      return currency_type::xrpl_trustline;
  }
  if (c.code_ == "USD" && c.issuer_ == "")
  {
    return currency_type::usd_unknown;
  }
  if (c.issuer_ != "")
  {
    currency::trustlines.push_back(c);
    return currency_type::xrpl_trustline;
  }
  //
  return currency_type::other;
}
*/
// ----------------------------------------------------------------------------
// currency get_currency(std::string_view code)
// {
//   auto icfn = [](std::string_view c) -> issued_currency {
//     if (c == "USD" || c == "EUR")
//       return issued_currency{currency::bitstamp_trust, std::string{c}};
//     if (c == "XRP")
//       return issued_currency{"", "XRP"};
//     else
//       return issued_currency{"", std::string{c}};
//   };
//   auto ic1 = icfn(code);
//   return currency{ic1, get_currency_type(ic1), 0, 0, 0, nullptr};
// }

// ----------------------------------------------------------------------------
currency get_currency(std::string_view issuer, std::string_view code)
{
  assert(std::string(code) != std::string(""));
  return currency(issued_currency{std::string{issuer}, std::string{code}});
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
/*
std::pair<std::string, std::string> to_string(currency_type const& t)
{
  switch (t)
  {
  case xrp:
    return std::make_pair("XRP", "");
    break;
  case usd_bitstamp:
    return std::make_pair("USD", currency::bitstamp_trust);
    break;
  case eur_bitstamp:
    return std::make_pair("EUR", currency::bitstamp_trust);
    break;
  case usd_gatehub:
    return std::make_pair("USD", currency::gatehub_trust);
    break;
  case xrpl_trustline:
    throw std::runtime_error("Insert search of trustlines here");
  case other:
    return std::make_pair("other", "");
    break;
  default:
    return std::make_pair("Unknown", "");
    break;
  }
  return std::make_pair("error", "");
}
*/
// ----------------------------------------------------------------------------
std::pair<std::string, std::string> to_string(currency const& c)
{
  return std::make_pair(c.curr_.issuer_, c.curr_.code_);

  // switch (t.type_)
  // {
  // case xrp:
  //   return std::make_pair("XRP", "");
  //   break;
  // case usd_bitstamp:
  //   return std::make_pair("USD", currency::bitstamp_trust);
  //   break;
  // case eur_bitstamp:
  //   return std::make_pair("EUR", currency::bitstamp_trust);
  //   break;
  // case usd_gatehub:
  //   return std::make_pair("USD", currency::gatehub_trust);
  //   break;
  // case xrpl_trustline:
  //   return std::make_pair(t.curr_.code_, t.curr_.issuer_);
  //   break;
  // case other:
  //   return std::make_pair("other", "");
  //   break;
  // default:
  //   return std::make_pair("Unknown", "");
  //   break;
  // }
  // return std::make_pair("error", "");
}

// ----------------------------------------------------------------------------
std::string currency_pair_string(currency_pair const& p, std::string_view sep)
{
  std::string str = std::get<0>(p).curr_.code_ + std::string(sep) + std::get<1>(p).curr_.code_;
  return str;
}

// ----------------------------------------------------------------------------
std::string currency_pair_lowercase_string(currency_pair const& p)
{
  std::string str = std::get<0>(p).curr_.code_ + std::get<1>(p).curr_.code_;
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
  std::string_view p1 = s.substr(0, e0);
  std::string_view p2 = s.substr(e1, s.back());
  return {::get_currency("", p1), ::get_currency("", p2)};
}

// ----------------------------------------------------------------------------
// std::ostream& operator<<(std::ostream& os, currency_type const& t)
// {
//   os << to_string(t).first;
//   return os;
// }

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, issued_currency const& c)
{
  if (c.is_fiat())
  {
    os << c.code_;
  }
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
  os << c.curr_.code_ << c.balance_;
  return os;
}

// ----------------------------------------------------------------------------
std::string to_string(double amount, currency const& c)
{
  int dec = 6;
  if (c.is_fiat())
  {
    dec = 2;
  }
  std::stringstream stream;
  stream << std::fixed << std::setprecision(dec) << amount;
  return stream.str();
}
