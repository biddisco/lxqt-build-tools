#pragma once

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// ----------------------------------------------------------------------------
struct currency_code
{
  using list = std::vector<currency_code>;
  //
  std::string issuer_;
  std::string code_;

  // --------------------------------------------------------------------------
  currency_code() = default;
  currency_code(currency_code const& c) = default;
  currency_code(std::string_view s);
  //
  currency_code(std::string_view issuer, std::string_view code)
    : issuer_{std::string(issuer)}
    , code_{std::string(code)}
  {
  }

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

  std::string issuer_name() const
  {
    auto name = currency_code::issuer_to_name.find(issuer_);
    if (name != currency_code::issuer_to_name.end())
      return name->second;
    else
      return issuer_;
  }

  std::string to_stringrep(bool add_issuer, bool shortname) const
  {
    if (add_issuer && (issuer_ != ""))
    {
      if (shortname)
        return code_ + "." + issuer_name();
      else
        return code_ + "." + issuer_;
    }
    return code_;
  }

  //
  static inline std::string const bitstamp_trust = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  static inline std::string const gatehub_trust = "rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq";
  static inline std::string const ripple_trust = "rMxCKbEDwqr76QuheSUMdEGf4B9xJ8m5De";

  static inline std::map<std::string, std::string> issuer_to_name = {
      {bitstamp_trust, "bitstamp"}, {gatehub_trust, "gatehub"}, {ripple_trust, "ripple"}};
  static inline std::map<std::string, std::string> name_to_issuer = {
      {"bitstamp", bitstamp_trust}, {"gatehub", gatehub_trust}, {"ripple", ripple_trust}};

  //        {"rHXuEaRYnnJHbDeuBH5w8yPh5uwNVh5zAg", "ELS"},
  //        {"rsoLo2S1kiGeCcn6hCUXVrCpGMWLrRrLZz", "SOLO"},
  //        {"raEQc5krJ2rUXyi6fgmUAf63oAXmF7p6jp", "ALV"},
  //        {"rM7zpZQBfz9y2jEkDrKcXiYPitJx9YTS1J", "DKP"},
  //        {"rBPtuMc4HBR1SuZyZv8hs7WBVxLBYrzxbY", "PASA"},
};

// ----------------------------------------------------------------------------
// currency_code string_to_code(std::string_view s);

// ----------------------------------------------------------------------------
// displays an amount such as 1.34 as a string, but uses different numbers
// of decimal places depending on the currency type (fiat always 2)
std::string currency_precision(double amount, currency_code const& c);

// ----------------------------------------------------------------------------
namespace currencies {
  static inline currency_code::list trustlines = {};
}    // namespace currencies
