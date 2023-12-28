#pragma once

#include <string>
#include <vector>
//
#include <QObject>
//
#include "currency/currency.hpp"
#include "currency/trade_data.hpp"
//
#include "data/ohlc_dataset_view.hpp"
#include "network/https-async.hpp"
#include "network/qwebsocket_client.hpp"
#include "network/qwebsocket_session.hpp"

class basic_account;

// ----------------------------------------------------------------------------
namespace network {
  enum streams : int
  {
    my_trades,
    my_orders,
    live_trades,
    order_book,
    accounts,
    invalid,
  };
}
using streams_vector = std::vector<network::streams>;

// ----------------------------------------------------------------------------
static std::string stream_to_text(network::streams stype)
{
  // always change stream_to_text and stream_from_text together
  switch (stype)
  {
  case network::streams::my_trades:
    return "My Trades";
  case network::streams::my_orders:
    return "My Orders";
  case network::streams::live_trades:
    return "Live Trades";
  case network::streams::order_book:
    return "Order Book";
  case network::streams::accounts:
    return "Account Changes";
  }
  return "Unknown";
}

static network::streams stream_from_text(std::string_view txt)
{
  // always change stream_to_text and stream_from_text together
  if (txt == "My Trades")
    return network::streams::my_trades;
  if (txt == "My Orders")
    return network::streams::my_orders;
  if (txt == "Live Trades")
    return network::streams::live_trades;
  if (txt == "Order Book")
    return network::streams::order_book;
  if (txt == "Account Changes")
    return network::streams::accounts;
  return network::streams::invalid;
}

// ----------------------------------------------------------------------------
class price_chart_widget;
class order_book_base;
class OrderBookPlot;
class QPlainTextEdit;

struct ticker_data
{
  std::shared_ptr<ohlc_dataset_view> view_;
  price_chart_widget* chart_widget_;
  order_book_base* orderbook_;
  QPlainTextEdit* orderbook_text_;
  OrderBookPlot* orderbook_plot_;
  // each ticker may subscribe to multiple streams
  std::map<network::streams, std::shared_ptr<net::ws::qwebsocket_session>> websockets_;
};

// To ensure Qt can emit signals of this type
Q_DECLARE_METATYPE(ticker_data)

// ----------------------------------------------------------------------------
class exchange
  : public QObject
  , public std::enable_shared_from_this<exchange>
{
  Q_OBJECT

  public:
  using exchange_vector = std::vector<std::shared_ptr<exchange>>;
  using exchange_map = std::map<currency_pair, ticker_data>;

  // websocket streams subscribed to format = ticker/stream_name
  std::map<std::string, bool> enabled_streams_;

  // ticker pairs available
  currency_pairlist tickers_available_;

  // ticker pairs subscribed to
  exchange_map tickers_subscribed_;

  // obligatory virtual destructor
  virtual ~exchange() {}

  // ---------------------------------------
  // concreate exchange instantiations must override the initialization
  // ---------------------------------------
  virtual void initialize() = 0;

  // ---------------------------------------
  // subscription to tickers
  // a ticker may be monitored via http get requests for candles
  // withut subscribing to any streams for live trades/other
  // ---------------------------------------
  // query which tickers (currency pairs) are subscribed
  virtual bool ticker_subscribed(currency const& c1, currency const& c2);
  virtual bool ticker_subscribed(std::string_view p1, std::string_view p2);
  // un/subscribe to a ticker
  virtual void ticker_subscribe(currency const& c1, currency const& c2);
  virtual void ticker_subscribe(std::string_view p1, std::string_view p2);
  virtual void ticker_unsubscribe(currency const& c1, currency const& c2);
  // return list of subscribed tickers
  exchange_map const& tickers_subscribed();

  // ---------------------------------------
  // websocket/stream connection management
  // a ticker may provide streams of dat which are subscribed to individually
  // ---------------------------------------
  // return a list of all streams available at the exchange level
  virtual streams_vector websocket_streams() = 0;

  // check if a particular ticker/stream is subscribed to
  virtual bool stream_subscribed(std::string const& s);
  // puts an entry into the stream map
  void mark_stream_subscribed(std::string const& s, bool enabled);
  // un/subscribe to an individual ticker stream
  virtual bool stream_subscribe(net::contexts& io_contexts, currency_pair const& cp,
    network::streams const stream, bool enabled) = 0;
  //  virtual bool websocket_connect(net::contexts& io_contexts, streams_vector const& streams) = 0;
  //  virtual bool websocket_disconnect(net::contexts& io_contexts, streams_vector const& streams) = 0;

  //
  virtual void shut_down() = 0;

  // ---------------------------------------
  // currency management
  // ---------------------------------------
  virtual bool can_send(currency& c, exchange* dest) = 0;
  virtual bool make_payment(currency& c, basic_account* src, basic_account* dest) = 0;
  virtual std::string_view name() = 0;
  virtual void cancel_order(trade_data const& t) = 0;
  virtual void place_buy_sell_orders(basic_account*, std::vector<trade_data> const&) = 0;
  virtual std::vector<basic_account*> wallets() = 0;

  // ---------------------------------------
  // setup / query tickers
  // ---------------------------------------
  virtual bool add_currency_pair(std::string_view p1, std::string_view p2) = 0;
  virtual currency_pairlist const& get_currency_pairs();

  // ---------------------------------------
  // fees
  // ---------------------------------------
  virtual double get_fee_percent(currency_type const& c1, currency_type const& c2) = 0;
  virtual double get_fee_fixed(currency_type const& c1, currency_type const& c2) = 0;
  virtual double get_transfer_fee(currency const& c1) = 0;
  virtual void custom_functions(basic_account* acct) = 0;

  signals:
  // emitted when a transaction might cause a change in data
  void transaction_event();
  void network_initialized(exchange* ex);
};
