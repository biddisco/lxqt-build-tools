#include <iomanip>
#include <sstream>
#include <string>
//
#include "currency/currency_code.hpp"

// ----------------------------------------------------------------------------
currency_code::currency_code(std::string_view s)
{
  auto e0 = s.find('.');
  if (e0 != std::string::npos)
  {
    auto e1 = e0 + 1;
    code_ = s.substr(0, e0);
    std::string issuer = std::string(s.substr(e1, s.back()));
    if (name_to_issuer.find(issuer) != name_to_issuer.end())
      issuer_ = name_to_issuer.at(issuer);
    else
      issuer_ = issuer;
  }
  else
  {
    code_ = s;
    issuer_ = "";
  }
}

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, currency_code const& c)
{
  if (c.is_fiat()) { os << c.code_; }
  else
  {
    auto name = currency_code::issuer_to_name.find(c.issuer_);
    if (name != currency_code::issuer_to_name.end())
      os << c.code_ << "." << name->second;
    else
      os << c.code_ << "." << c.issuer_;
  }
  return os;
}

// ----------------------------------------------------------------------------
std::string currency_precision(double amount, currency_code const& c)
{
  int dec = 6;
  if (c.is_fiat()) { dec = 2; }
  std::stringstream stream;
  stream << std::fixed << std::setprecision(dec) << amount;
  return stream.str();
}

// // ----------------------------------------------------------------------------
// currency_code string_to_code(std::string_view s)
// {
//   auto e0 = s.find('.');
//   if (e0 != std::string::npos)
//   {
//     auto e1 = e0 + 1;
//     std::string_view p0 = s.substr(0, e0);           // code
//     std::string_view p1 = s.substr(e1, s.back());    // issuer
//     return {p1, p0};
//   }
//   return {"", s};
// }
