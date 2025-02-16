#pragma once
//
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
//
#include <QObject>
#include <QTimer>
//
#include <magic_enum/magic_enum.hpp>
//
#include "currency/currency.hpp"
#include "currency/currency_pair.hpp"
#include "currency/json_data_types.hpp"
#include "currency/trade_data.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "exchange/ticker_data.hpp"
#include "network/qwebsocket_client.hpp"
#include "network/qwebsocket_session.hpp"
#include "senders/sender_defs.hpp"
#include "util/pubsub.hpp"

class basic_account;

// ----------------------------------------------------------------------------
class abstract_exchange
  : public QObject
  , public std::enable_shared_from_this<abstract_exchange>
{
  Q_OBJECT

  public:
  using exchange_vector = std::vector<std::shared_ptr<abstract_exchange>>;
  using exchange_map = std::map<currency_pair, ticker::data>;
  using factory_function = std::function<void(currency_pair, ticker::data, ticker::streams)>;

  // websocket streams subscribed to format = ticker/stream_name
  std::map<std::string, bool> enabled_streams_;

  // a place that stores registeered factory functions (callbacks)
  // these are used for subscription/unsubscription
  std::map<std::string, factory_function> factories_;

  // ticker pairs available
  currency_pair::list tickers_available_;

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
  abstract_exchange();

  // ---------------------------------------
  // obligatory virtual destructor
  virtual ~abstract_exchange();

  // ---------------------------------------
  // concreate abstract_exchange instantiations must override the initialization
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
  // return a list of all streams available at the abstract_exchange level
  virtual stream_set websocket_streams() = 0;

  // check if a particular ticker/stream is subscribed to
  virtual bool is_stream_subscribed(currency_pair cp, ticker::streams s);
  // puts an entry into the stream map
  void mark_stream_subscribed(currency_pair cp, ticker::streams s, bool enabled);
  // un/subscribe to an individual ticker stream
  virtual bool stream_subscribe(
      currency_pair const& cp, ticker::streams const stream, bool enabled, factory_function f) = 0;

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
  ticker::data get_subscribed_ticker_data(currency_pair cp) const;

  // ---------------------------------------
  // setup / query tickers
  // ---------------------------------------
  virtual bool add_currency_pair(currency_pair const& cp);
  virtual currency_pair::list const& get_currency_pairs() const;

  // ---------------------------------------
  // currency management
  // ---------------------------------------
  virtual bool can_send(currency_code const& c, abstract_exchange* dest) = 0;
  virtual bool make_payment(currency_amount const& c, basic_account* src, basic_account* dest) = 0;
  virtual std::string get_name() const { return exchange_name_; }
  virtual any_bytearray_sender request_cancel_order(trade_data const& t) = 0;
  virtual void place_buy_sell_orders(basic_account*, std::vector<trade_data> const&) = 0;
  virtual std::vector<basic_account*> wallets() = 0;

  // ---------------------------------------
  // shut down
  // ---------------------------------------
  virtual void shut_down();

  // ---------------------------------------
  // fees
  // ---------------------------------------
  virtual ticker::transaction_fees get_fees(currency_pair const& cp) = 0;

  // ---------------------------------------
  // fees
  // ---------------------------------------
  virtual void custom_functions(basic_account* acct) = 0;

  Q_SIGNALS:
  // emitted when a transaction might cause a change in data
  void transaction_event();
  void network_initialized(abstract_exchange* ex);
};
