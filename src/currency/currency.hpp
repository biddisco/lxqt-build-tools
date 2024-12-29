#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//
#include <QString>

//        {"rHXuEaRYnnJHbDeuBH5w8yPh5uwNVh5zAg", "ELS"},
//        {"rsoLo2S1kiGeCcn6hCUXVrCpGMWLrRrLZz", "SOLO"},
//        {"raEQc5krJ2rUXyi6fgmUAf63oAXmF7p6jp", "ALV"},
//        {"rM7zpZQBfz9y2jEkDrKcXiYPitJx9YTS1J", "DKP"},
//        {"rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY", "1"},
//        {"rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY", "PASA"},

class currency_widget;

// ----------------------------------------------------------------------------
std::string currency_to_hex(std::string_view name);
std::string hex_to_currency(std::string_view name);

// ----------------------------------------------------------------------------
struct currency_code
{
  static inline std::string const bitstamp_trust = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  static inline std::string const gatehub_trust = "rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq";
  static inline std::string const ripple_trust = "rMxCKbEDwqr76QuheSUMdEGf4B9xJ8m5De";
  static inline std::vector<currency_code> trustlines = {};
  //
  std::string issuer_;
  std::string code_;

  bool operator==(currency_code const& c) const
  {
    return (code_ == c.code_) && (issuer_ == c.issuer_);
  }
  bool operator<(currency_code const& c) const
  {
    return (code_ < c.code_) || ((code_ == c.code_) && (issuer_ < c.issuer_));
  }
  bool operator>(currency_code const& c) const
  {
    return (code_ > c.code_) || ((code_ == c.code_) && (issuer_ > c.issuer_));
  }

  // return true if the currency is a fiat currency such as USD, EUR etc etc
  bool is_fiat() const { return (issuer_ == ""); }
  bool is_xrp() const { return (issuer_ == "") && (code_ == "XRP"); }

  // stream operators
  friend std::ostream& operator<<(std::ostream&, currency_code const&);

  std::string to_stringrep(bool add_issuer = true) const
  {
    if (add_issuer && (issuer_ != "")) return code_ + "." + issuer_;
    return code_;
  }
};

// ----------------------------------------------------------------------------
struct currency : public currency_code
{
  currency() = default;

  currency(std::string_view issuer, std::string_view code)
    : currency_code({std::string(issuer), std::string(code)})
    , balance_(0)
    , avail_(0)
    , reserved_(0)
    , widget_(nullptr)
  {
  }

  currency(currency_code const& c)
    : currency_code(c)
    , balance_(0)
    , avail_(0)
    , reserved_(0)
    , widget_(nullptr)
  {
  }

  currency(
      currency_code curr, double balance, double avail, double reserved, currency_widget* widget)
    : currency_code(curr)
    , balance_(balance)
    , avail_(avail)
    , reserved_(reserved)
    , widget_(widget)
  {
  }

  // comparison operators
  bool operator==(currency const& c) const { return (code_ == c.code_) && (issuer_ == c.issuer_); }

  // bool operator<(currency const& c) const { return code_ < c.code_; }
  // bool operator>(currency const& c) const { return code_ > c.code_; }

  // stream operators
  friend std::ostream& operator<<(std::ostream&, currency const&);

  double balance_;
  double avail_;
  double reserved_;
  currency_widget* widget_;
};

// ----------------------------------------------------------------------------
currency string_to_code(std::string_view s);

// ----------------------------------------------------------------------------
// displays an amount such as 1.34 as a string, but uses different numbers
// of decimal places depending on the currency type (fiat always 2)
std::string to_string(double amount, currency const& c);

template <typename T>
std::string to_string_with_precision(T const a_value, int const n = 6)
{
  std::ostringstream out;
  out.precision(n);
  out << std::fixed << a_value;
  return out.str();
}
