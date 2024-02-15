#include <QObject>
//
#include <range/v3/algorithm.hpp>
#include "debug/print.hpp"
#include "exchange/exchange.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> exchange_dbg("Exchange");

// ----------------------------------------------------------------------------
bool exchange::is_stream_subscribed(currency_pair cp, network::streams s)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  if (enabled_streams_.contains(key))
  {
    return enabled_streams_[key];
  }
  return false;
}

// ----------------------------------------------------------------------------
void exchange::mark_stream_subscribed(currency_pair cp, network::streams s, bool enabled)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  enabled_streams_[key] = enabled;
}

// ----------------------------------------------------------------------------
exchange::exchange_map const& exchange::tickers_subscribed() const
{
  return tickers_subscribed_;
}

exchange::exchange_map& exchange::tickers_subscribed()
{
  return tickers_subscribed_;
}

// ----------------------------------------------------------------------------
bool exchange::ticker_subscribed(currency const& c1, currency const& c2)
{
  auto present = (tickers_subscribed_.contains(currency_pair{c1, c2}));
  return present;
}

// ----------------------------------------------------------------------------
stream_set exchange::ticker_subscribe(currency const& c1, currency const& c2)
{
  throw std::runtime_error("Exchange classes must implement this function");
}

// ----------------------------------------------------------------------------
stream_set exchange::ticker_subscribe(const currency_pair& p)
{
  return ticker_subscribe(std::get<0>(p), std::get<1>(p));
}

// ----------------------------------------------------------------------------
void exchange::ticker_unsubscribe(currency const& c1, currency const& c2)
{
  if (ticker_subscribed(c1, c2))
  {
    tickers_subscribed_.erase(currency_pair{c1, c2});
  }
}

// ----------------------------------------------------------------------------
currency_pairlist const& exchange::get_currency_pairs()
{
  return tickers_available_;
}

// ----------------------------------------------------------------------------
bool exchange::add_currency_pair(const currency& c1, const currency& c2)
{
  tickers_available_.push_back(std::make_pair(c1, c2));
  return true;
}
