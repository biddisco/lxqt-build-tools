#pragma once
// stl
#include <set>
#include <string>
#include <vector>
// qt
#include <QObject>
#include <QTimer>
// extern
#include <magic_enum.hpp>
// grox
#include "currency/currency.hpp"
#include "currency/json_data_types.hpp"
#include "currency/trade_data.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "network/qwebsocket_client.hpp"
#include "network/qwebsocket_session.hpp"
#include "util/pubsub.hpp"

class basic_account;
class exchange;

// ----------------------------------------------------------------------------
namespace network {
  enum streams : int
  {
    my_trades = 0,
    my_orders,
    live_trades,
    order_book,
    price_data,
    account_changes,
    invalid,
  };
  constexpr auto stream_names = magic_enum::enum_names<network::streams>();
}    // namespace network

using stream_set = std::set<network::streams>;

// ----------------------------------------------------------------------------
static std::string stream_to_pretty_text(network::streams stream)
{
  std::string txt = std::string(magic_enum::enum_name(stream));
  // Transform fir char after each break
  txt[0] = std::toupper(txt[0]);
  std::for_each(txt.begin() + 1, txt.end(), [](char& c) {
    if ((*(&c - 1)) == '_') c = std::toupper(c);
  });
  std::transform(txt.begin(), txt.end(), txt.begin(), [](char& c) { return (c == '_') ? ' ' : c; });
  return txt;
}

static network::streams stream_from_pretty_text(std::string txt)
{
  std::transform(txt.begin(), txt.end(), txt.begin(), [](char c) { return std::tolower(c); });
  auto stream = magic_enum::enum_cast<network::streams>(txt);
  if (stream.has_value()) { return stream.value(); }
  return network::streams::invalid;
}

// ----------------------------------------------------------------------------
class price_chart_widget;
class order_book_base;

struct ticker_subscription
{
  std::shared_ptr<exchange> exchange_;
  std::shared_ptr<ohlc_dataset_view> view_;
  std::shared_ptr<order_book_base> orderbook_;
  std::shared_ptr<price_chart_widget> chart_widget_;
  // each ticker may subscribe to multiple streams
  std::map<network::streams, std::shared_ptr<net::ws::qwebsocket_session>> websockets_;
  //
  grox::PublishSubscribe<const currency_pair, const grox::live_trade_data> live_trade_subscribers_;
  grox::PublishSubscribe<currency_pair const> orderbook_subscribers_;
  grox::PublishSubscribe<candle_res const> new_ohlc_subscribers_;
};

using ticker_data = std::shared_ptr<ticker_subscription>;

// ----------------------------------------------------------------------------
class exchange
  : public QObject
  , public std::enable_shared_from_this<exchange>
{
  Q_OBJECT

  public:
  using exchange_vector = std::vector<std::shared_ptr<exchange>>;
  using exchange_map = std::map<currency_pair, ticker_data>;
  using factory_function = std::function<void(currency_pair, ticker_data, network::streams)>;

  // websocket streams subscribed to format = ticker/stream_name
  std::map<std::string, bool> enabled_streams_;

  // a place that stores registeered factory functions (callbacks)
  // these are used for subscription/unsubscription
  std::map<std::string, factory_function> factories_;

  // ticker pairs available
  currency_pairlist tickers_available_;

  // ticker pairs subscribed to
  exchange_map tickers_subscribed_;

  std::string exchange_name_;
  QTimer* timer_;

  public:
  // if an asynchronous websocket/http operation is being handled
  // then shutdown must wait until it has completed before starting
  // and then set a flag to prevent new async operations being handled
  std::mutex async_mutex_;
  std::atomic<bool> closing_down_;

  public:
  // ---------------------------------------
  exchange();

  // ---------------------------------------
  // obligatory virtual destructor
  virtual ~exchange();

  // ---------------------------------------
  // concreate exchange instantiations must override the initialization
  // ---------------------------------------
  virtual void initialize() = 0;

  // ---------------------------------------
  // timer used for http/other updates
  // ---------------------------------------
  QTimer* get_clock_timer() { return timer_; }

  // ---------------------------------------
  // factory functions to be used for callbacks to stream subscribe/unsubscribe events
  // ---------------------------------------
  void register_factory(std::string name, factory_function f);
  factory_function get_factory(std::string name);

  // ---------------------------------------
  // websocket/stream connection management
  // a ticker may provide streams of dat which are subscribed to individually
  // ---------------------------------------
  // return a list of all streams available at the exchange level
  virtual stream_set websocket_streams() = 0;

  // check if a particular ticker/stream is subscribed to
  virtual bool is_stream_subscribed(currency_pair cp, network::streams s);
  // puts an entry into the stream map
  void mark_stream_subscribed(currency_pair cp, network::streams s, bool enabled);
  // un/subscribe to an individual ticker stream
  virtual bool stream_subscribe(
      currency_pair const& cp, network::streams const stream, bool enabled, factory_function f) = 0;

  //  virtual bool websocket_connect(net::contexts& io_contexts, stream_set const& streams) = 0;
  //  virtual bool websocket_disconnect(net::contexts& io_contexts, stream_set const& streams) = 0;

  // ---------------------------------------
  // subscription to tickers
  // a ticker may be monitored via http get requests for candles
  // withut subscribing to any streams for live trades/other
  // ---------------------------------------
  // query which tickers (currency pairs) are subscribed
  virtual bool ticker_subscribed(currency_pair const& cp);
  // un/subscribe to a ticker
  virtual stream_set ticker_subscribe(currency_pair const& cp);
  virtual void ticker_unsubscribe(currency_pair const& cp);
  // return list of subscribed tickers
  exchange_map const& tickers_subscribed() const;
  // exchange_map& tickers_subscribed();
  ticker_data get_subscribed_ticker_data(currency_pair cp) const;

  // ---------------------------------------
  // setup / query tickers
  // ---------------------------------------
  virtual bool add_currency_pair(currency_pair const& cp);
  virtual currency_pairlist const& get_currency_pairs();

  // ---------------------------------------
  // currency management
  // ---------------------------------------
  virtual bool can_send(currency const& c, exchange* dest) = 0;
  virtual bool make_payment(currency const& c, basic_account* src, basic_account* dest) = 0;
  virtual std::string get_name() const { return exchange_name_; }
  virtual void cancel_order(trade_data const& t) = 0;
  virtual void place_buy_sell_orders(basic_account*, std::vector<trade_data> const&) = 0;
  virtual std::vector<basic_account*> wallets() = 0;

  // ---------------------------------------
  // shut down
  // ---------------------------------------
  virtual void shut_down() = 0;

  // ---------------------------------------
  // fees
  // ---------------------------------------
  virtual double get_fee_percent(currency_pair const& cp) = 0;
  virtual double get_fee_fixed(currency_pair const& cp) = 0;
  virtual double get_transfer_fee(currency const& c1) = 0;
  virtual void custom_functions(basic_account* acct) = 0;

  Q_SIGNALS:
  // emitted when a transaction might cause a change in data
  void transaction_event();
  void network_initialized(exchange* ex);
};
