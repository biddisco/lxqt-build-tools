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
inline constexpr print_threshold<Level, debug_level> exchange_dbg("Exchange");

// ----------------------------------------------------------------------------
exchange::exchange()
{
  timer_ = new QTimer(nullptr);
  timer_->start(1000);
  //
  closing_down_ = false;
}

// ----------------------------------------------------------------------------
exchange::~exchange()
{
  delete timer_;
  tickers_available_.clear();
}

// ----------------------------------------------------------------------------
void exchange::shut_down()
{
  // do not allow shutdown / async operations concurrently
  closing_down_ = true;
  std::lock_guard l(async_mutex_);
  exchange_dbg<0>.debug(str<>(get_name().c_str()), "shutdown start");
  //
  for (auto& [ticker, tdata] : tickers_subscribed_)
  {
    for (auto& [stream, websocket] : tdata->websockets_)
    {
      try
      {
        exchange_dbg<0>.debug(
            str<>("websocket reset"), currency_pair_string(ticker), fmt::ptr(websocket.get()));
        websocket.reset();
      }
      catch (std::exception const& e)
      {
        std::cerr << e.what() << std::endl;
      }
    }
    // delete orderbook _after_ closing websocket to avoid some late async data arrivals
    tdata->orderbook_ = nullptr;
  }
  tickers_subscribed_.clear();
  //
  exchange_dbg<0>.debug(str<>(get_name().c_str()), "shutdown complete");
}

// ----------------------------------------------------------------------------
void exchange::register_factory(std::string name, exchange::factory_function f)
{
  if (factories_.contains(name)) throw std::runtime_error("Duplicate factory registration");
  factories_[name] = f;
}

// ----------------------------------------------------------------------------
exchange::factory_function exchange::get_factory(std::string name)
{
  if (!factories_.contains(name)) throw std::runtime_error("Invalid factory retrieval");
  return factories_[name];
}

// ----------------------------------------------------------------------------
bool exchange::is_stream_subscribed(currency_pair cp, network::streams s)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  if (enabled_streams_.contains(key)) { return enabled_streams_[key]; }
  return false;
}

// ----------------------------------------------------------------------------
void exchange::mark_stream_subscribed(currency_pair cp, network::streams s, bool enabled)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  enabled_streams_[key] = enabled;
}

// ----------------------------------------------------------------------------
ticker_data exchange::get_subscribed_ticker_data(currency_pair cp) const
{
  if (tickers_subscribed_.contains(cp)) { return tickers_subscribed_.at(cp); }
  else
    throw std::runtime_error("Attempt to access unsubscribed ticker");
}

// ----------------------------------------------------------------------------
exchange::exchange_map const& exchange::tickers_subscribed() const { return tickers_subscribed_; }

/*
exchange::exchange_map& exchange::tickers_subscribed()
{
  return tickers_subscribed_;
}
*/
// ----------------------------------------------------------------------------
bool exchange::ticker_subscribed(currency_pair const& cp)
{
  auto present = (tickers_subscribed_.contains(cp));
  return present;
}

// ----------------------------------------------------------------------------
stream_set exchange::ticker_subscribe(currency_pair const& cp)
{
  throw std::runtime_error("Exchange classes must implement this function");
}

// ----------------------------------------------------------------------------
void exchange::ticker_unsubscribe(currency_pair const& cp)
{
  if (ticker_subscribed(cp)) { tickers_subscribed_.erase(cp); }
}

// ----------------------------------------------------------------------------
currency_pairlist const& exchange::get_currency_pairs() const { return tickers_available_; }

// ----------------------------------------------------------------------------
bool exchange::add_currency_pair(currency_pair const& cp)
{
  tickers_available_.push_back(cp);
  return true;
}
