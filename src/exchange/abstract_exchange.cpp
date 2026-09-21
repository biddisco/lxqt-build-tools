#include <iostream>
#include <string>
//
#include <range/v3/algorithm.hpp>
#include <fmt/format.h>
//
#include <QObject>
//
#include "config/config.hpp"
#include "debug/logging.hpp"
#include "exchange/abstract_exchange.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
static auto exchange_log = grox::log::create("Exchange");

// ----------------------------------------------------------------------------
abstract_exchange::abstract_exchange()
{
  timer_ = new QTimer(nullptr);
  timer_->start(1000);
  //
  closing_down_ = false;
}

// ----------------------------------------------------------------------------
abstract_exchange::~abstract_exchange()
{
  delete timer_;
  tickers_available_.clear();
}

// ----------------------------------------------------------------------------
void abstract_exchange::shut_down()
{
  // do not allow shutdown / async operations concurrently
  closing_down_ = true;
  std::lock_guard l(async_mutex_);
  GROX_LOG_DEBUG(exchange_log, "{:>20} shutdown start", get_name());
  //
  save_subscribed_tickers();
  //
  for (auto& [ticker, tdata] : tickers_subscribed_)
  {
    for (auto& [stream, websocket] : tdata->websockets_)
    {
      try
      {
        GROX_LOG_DEBUG(exchange_log, "{:>20} {} {}", "websocket reset",
            currency_pair_string(ticker), fmt::ptr(websocket.get()));
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
  GROX_LOG_DEBUG(exchange_log, "{:>20} shutdown complete", get_name());
}

// ----------------------------------------------------------------------------
void abstract_exchange::register_factory(std::string name, abstract_exchange::factory_function f)
{
  if (factories_.contains(name)) throw std::runtime_error("Duplicate factory registration");
  factories_[name] = f;
}

// ----------------------------------------------------------------------------
abstract_exchange::factory_function abstract_exchange::get_factory(std::string name)
{
  if (!factories_.contains(name)) throw std::runtime_error("Invalid factory retrieval");
  return factories_[name];
}

// ----------------------------------------------------------------------------
void abstract_exchange::add_subscribed_ticker(currency_pair const& cp, ticker::data const data)
{
  tickers_subscribed_.insert({cp, data});
}

// ----------------------------------------------------------------------------
bool abstract_exchange::is_stream_subscribed(currency_pair cp, ticker::streams s)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  if (enabled_streams_.contains(key)) { return enabled_streams_[key]; }
  return false;
}

// ----------------------------------------------------------------------------
void abstract_exchange::mark_stream_subscribed(currency_pair cp, ticker::streams s, bool enabled)
{
  std::string key = currency_pair_string(cp) + "/" + std::string(magic_enum::enum_name(s));
  enabled_streams_[key] = enabled;
}

// ----------------------------------------------------------------------------
ticker::data abstract_exchange::get_subscribed_ticker_data(currency_pair cp) const
{
  auto l = take_readonly_lock();
  if (tickers_subscribed_.contains(cp)) { return tickers_subscribed_.at(cp); }
  else
  {
    for (auto const& [key, value] : tickers_subscribed_)
    {
      GROX_LOG_ERROR(exchange_log, "{:>20} want {} Found {}", "tickers_subscribed",
          currency_pair_string(cp), currency_pair_string(key));
    }
    throw std::runtime_error("Attempt to access unsubscribed ticker");
  }
}

// ----------------------------------------------------------------------------
ticker::data abstract_exchange::ensure_orderbook_subscribed(currency_pair const& cp)
{
  // Subscribe the ticker (idempotent — exchange impls early-return when
  // already subscribed).
  if (!ticker_subscribed(cp)) ticker_subscribe(cp);

  // Subscribe the order_book stream if not already subscribed. We pass a
  // no-op factory so no per-subscription GUI dockwidget is created — the
  // trading widget built later owns its own UI.
  if (!is_stream_subscribed(cp, ticker::streams::order_book))
  {
    factory_function noop = [](currency_pair, ticker::data, ticker::streams) {};
    stream_subscribe(cp, ticker::streams::order_book, true, noop);
  }

  return get_subscribed_ticker_data(cp);
}

// ----------------------------------------------------------------------------
abstract_exchange::exchange_map const& abstract_exchange::tickers_subscribed(
    subscription_lock_type& l) const
{
  l = take_readonly_lock();
  return tickers_subscribed_;
}

// ----------------------------------------------------------------------------
bool abstract_exchange::ticker_subscribed(currency_pair const& cp)
{
  auto l = take_readonly_lock();
  auto present = (tickers_subscribed_.contains(cp));
  return present;
}

// ----------------------------------------------------------------------------
stream_set abstract_exchange::ticker_subscribe(currency_pair const& cp)
{
  throw std::runtime_error("Exchange classes must implement this function");
}

// ----------------------------------------------------------------------------
void abstract_exchange::ticker_unsubscribe(currency_pair const& cp)
{
  auto l = take_readwrite_lock();
  if (ticker_subscribed(cp)) { tickers_subscribed_.erase(cp); }
}

// ----------------------------------------------------------------------------
currency_pair::list const& abstract_exchange::get_currency_pairs() const
{
  return tickers_available_;
}

// ----------------------------------------------------------------------------
bool abstract_exchange::add_currency_pair(currency_pair const& cp)
{
  tickers_available_.push_back(cp);
  return true;
}

// ----------------------------------------------------------------------------
void abstract_exchange::load_subscribed_tickers()
{
  // open ini file and get the group subscribed tickers
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // open global settings streams section
  settings.beginGroup("Tickers");
  // open group for this abstract_exchange
  settings.beginGroup(get_name());
  // get all subscribed tickers on this abstract_exchange from ini file
  for (auto const& ticker : settings.childKeys())
  {
    if (settings.value(ticker).toBool())
    {
      GROX_LOG_DEBUG(exchange_log, "{:>20} {}", "subscription", ticker.toStdString());
      currency_pair cp = string_to_pair(ticker.toStdString(), "-");
      ticker_subscribe(cp);
    }
  }
  settings.endGroup();
  settings.endGroup();
}

// ----------------------------------------------------------------------------
void abstract_exchange::save_subscribed_tickers()
{
  // open ini file and get the group subscribed tickers
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // open global settings streams section
  settings.beginGroup("Tickers");
  // open group for this abstract_exchange
  settings.beginGroup(get_name());

  // for each ticker we are subscribed to
  auto l = take_readonly_lock();
  for (auto const& ticker : tickers_subscribed_)
  {
    auto cp = ticker.first;
    std::string key = currency_pair_string(cp);
    settings.setValue(to_qstring(key), true);
  }
  settings.endGroup();
  settings.endGroup();
}
