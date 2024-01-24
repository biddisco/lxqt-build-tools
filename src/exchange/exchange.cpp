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
bool exchange::stream_subscribed(std::string const& s)
{
  if (enabled_streams_.find(s) != enabled_streams_.end())
  {
    return enabled_streams_[s];
  }
  return false;
}

// ----------------------------------------------------------------------------
void exchange::mark_stream_subscribed(std::string const& s, bool enabled)
{
  enabled_streams_[s] = enabled;
}

// ----------------------------------------------------------------------------
currency_pairlist const& exchange::get_currency_pairs()
{
  return tickers_available_;
}

// ----------------------------------------------------------------------------
exchange::exchange_map const& exchange::tickers_subscribed()
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
void exchange::ticker_subscribe(currency const& c1, currency const& c2)
{
  throw std::runtime_error("Exchange classes must implement this function");
}

// ----------------------------------------------------------------------------
void exchange::ticker_subscribe(const currency_pair& p)
{
  ticker_subscribe(std::get<0>(p), std::get<1>(p));
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
bool exchange::add_currency_pair(const currency& c1, const currency& c2)
{
  tickers_available_.push_back(std::make_pair(c1, c2));
  return true;
}
