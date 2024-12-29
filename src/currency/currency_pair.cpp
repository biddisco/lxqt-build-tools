#include <algorithm>
#include <sstream>
#include <string>
//
#include <QString>
//
#include "currency/currency.hpp"
#include "currency/currency_pair.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, currency_pair const& cp)
{
  os << currency_pair_string(cp);
  return os;
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
std::string currency_pair_string(currency_pair const& p, std::string_view sep, bool add_issuer)
{
  std::string s0 = p.c1_.to_stringrep(add_issuer);
  std::string s1 = p.c2_.to_stringrep(add_issuer);
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
  std::string str = p.c1_.code_ + p.c2_.code_;
  lowercase_i(str);
  return str;
}

// ----------------------------------------------------------------------------
currency_pair reverse_pair(currency_pair const& cp) { return {cp.c2_, cp.c1_}; }
