#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
//
#include <QString>
//
#include <exec/any_sender_of.hpp>
#include <exec/async_scope.hpp>
#include <stdexec/execution.hpp>
//
#include "currency/trade_data.hpp"
#include "exchange/account.hpp"
#include "exchange/exchange.hpp"
#include "exchange/order_book.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "senders/sender_defs.hpp"

// ----------------------------------------------------------------------------
class bitstamp_network : public exchange
{
  Q_OBJECT

  private:
  // websocket token / user id valid for N seconds
  std::string websocket_token_;
  // websocket token userid
  std::string websocket_user_id_;
  // token expiry time
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> token_expiry_;

  // usually only one present, but allow for more
  std::vector<bitstamp_account> accounts_;

  // map of fees for trading of currency pairs
  std::map<std::pair<std::string, std::string>, double> fee_map_;

  std::mutex candlestick_mutex_;
  std::set<currency_pair> candlestick_updates_active_;

  public:
  // if an asynchronous websocket/http operation is being handled
  // then shutdown must wait until it has completed before starting
  // and then set a flag to prevent new async operations being handled
  std::mutex async_mutex_;
  static std::atomic<bool> closing_down_;

  public:
  //
  static inline const std::string bitstamp_https_address = "www.bitstamp.net";
  static inline const int bitstamp_https_port = 443;
  //
  static inline const std::string bitstamp_websocket_address = "ws.bitstamp.net";
  static inline const int bitstamp_websocket_port = 443;

  public:
  // ---------------------------------------
  // singleton access to network/testnet
  // ---------------------------------------
  static std::shared_ptr<exchange> get_instance()
  {
    static std::shared_ptr<exchange> bitstamp_ptr = nullptr;
    if (bitstamp_ptr == nullptr)
      bitstamp_ptr = std::make_shared<bitstamp_network>();
    return bitstamp_ptr;
  }

  static std::shared_ptr<bitstamp_network> get_bitstamp_instance()
  {
    return std::dynamic_pointer_cast<bitstamp_network>(get_instance());
  }

  // ---------------------------------------
  // construct/destruct
  // ---------------------------------------
  bitstamp_network();
  ~bitstamp_network() override;

  void initialize() override;

  // returns a temp vector of account pointers (references)
  // to be used with caution because adding a wallet will
  // invalidate the pointer references
  std::vector<basic_account*> wallets() override
  {
    std::vector<basic_account*> accts;
    for (auto& acct : accounts_)
    {
      accts.push_back(&acct);
    }
    return accts;
  }

  bitstamp_account& account()
  {
    return accounts_[0];
  }

  // ---------------------------------------
  // network name
  std::string_view name() override
  {
    return "Bitstamp";
  }

  // Is sending this currency to the destination exchange supported
  bool can_send(const currency& c, exchange* dest) override;

  // ---------------------------------------
  // return the order book for this exchange
  bitstamp_order_book const& get_orderbook(currency_pair const& cp) const;

  // ---------------------------------------
  // init connections/websockets etc
  bool subscribe_live_trades(currency_pair const& cp, bool enable);
  bool subscribe_order_book(currency_pair const& cp, bool enable);
  bool subscribe_my_trades(currency_pair const& cp, bool enable);
  bool subscribe_my_orders(currency_pair const& cp, bool enable);
  //  bool unsubscribe_my_trades(currency_pair const& cp);
  //  bool unsubscribe_my_orders(currency_pair const& cp);

  streams_vector websocket_streams() override
  {
    return {
      network::streams::my_orders,      // private orders
      network::streams::my_trades,      // private trades
      network::streams::order_book,     // all orders
      network::streams::live_trades,    // all trades
    };
  }

  // connect to a single stream
  bool stream_subscribe(
    currency_pair const& cp, network::streams const stream, bool enabled) override;

  // connect to (multiple) streams
  //  bool websocket_connect(net::contexts& io_contexts, streams_vector const&
  //  streams) override; bool websocket_disconnect(net::contexts& io_contexts,
  //  streams_vector const& streams) override;

  // shut down sockets/connections
  void shut_down() override;

  // ---------------------------------------
  // http: get account info/data
  any_bytearray_sender request_account_info();
  // http: get new websocket token to subscribe to streams
  any_bytearray_sender request_websocket_token();
  // http: get open order data
  any_bytearray_sender request_open_orders();
  // http: get currency tickers available
  any_bytearray_sender request_tickers_available();

  // process account info response
  void handle_account_info(std::string_view);
  void handle_websocket_token(std::string_view);
  void handle_tickers_available(std::string_view);

  // ---------------------------------------
  // process open order data response
  void handle_open_orders(std::string_view);
  void process_order(nlohmann::json& jdata, std::string_view event);

  // ---------------------------------------
  // make a payment/transfer from bitstamp
  bool make_payment(currency const& c, basic_account* src, basic_account* dest) override;

  // ---------------------------------------
  // place a buy/sell order
  void place_limit_order(trade_data const& t, bool update_after);
  void place_buy_sell_orders(basic_account* acct, std::vector<trade_data> const& trades) override;
  void cancel_order(trade_data const& t) override;

  // ----------------------------------------------------------------------------
  net::http::client_ptr signed_request(const std::string& url_path, const std::string& url_query);

  // ----------------------------------------------------------------------------
  // OHLC candlestick updating
  // ----------------------------------------------------------------------------
  // triggers an update for all subscribed tickers
  // typically called once per minute by the application to update data
  // regularly
  void update_ohlc_datasets();
  // triggers an update for a single ticker
  void update_ohlc_data(currency_pair cp, ticker_data* data);
  // http : generate a request for candlestick data for a single ticker
  any_bytearray_sender request_new_ohlc_data(currency_pair cp, uint64_t start_t, uint64_t samples);
  // handler for an http request containing new data
  void handle_new_ohlc_data(ticker_data*, std::string_view);
  //
  any_bytearray_sender request_price_history(currency_pair cp);
  std::uint64_t handle_price_history(std::string_view data);

  // function called from websocket subscription to live trade data
  static void new_live_trade_data_q(bitstamp_network*, currency_pair cp, const QString);

  // function called from websocket subscription to live orderbook data
  static void new_orderbook_data_q(bitstamp_network*, currency_pair const cp, const QString);

  double get_fee_percent(currency const& c1, currency const& c2) override;
  double get_fee_fixed(currency const& c1, currency const& c2) override;
  double get_transfer_fee(currency const& /*c1*/) override
  {
    return 0;
  }

  void custom_functions(basic_account* /*acct*/) override{};

  streams_vector ticker_subscribe(currency const& c1, currency const& c2) override;

  signals:

  // when the wallet widget needs to be updated with new data/currencies
  void update_wallet_widget(bitstamp_account*);

  // trigger this to restart the timer from a Qt thread
  void restart_candlestick_timer();

  // after new data is received, trigger this to update plots
  void new_ohlc_data(ticker_data*, double);

  public slots:
  void candlestick_timer_event();
  void new_ohlc_data_event(ticker_data*, double);
};
