#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
//
#include <QString>
//
#include <exec/any_sender_of.hpp>
#include <stdexec/execution.hpp>
//
#include "currency/trade_data.hpp"
#include "data/order_book.hpp"
#include "exchange/abstract_exchange.hpp"
#include "exchange/account.hpp"
#include "exchange/order_book_bitstamp.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "senders/sender_defs.hpp"

// ----------------------------------------------------------------------------
class bitstamp_network : public abstract_exchange
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
  // @TODO add read/write lock for these maps
  std::map<currency_pair, double> transaction_fee_map_;
  std::map<currency_code, double> withdrawal_fee_map_;

  std::mutex candlestick_mutex_;
  std::set<currency_pair> candlestick_updates_active_;

  public:
  //
  static inline std::string const bitstamp_https_address = "www.bitstamp.net";
  static inline int const bitstamp_https_port = 443;
  //
  static inline std::string const bitstamp_websocket_address = "ws.bitstamp.net";
  static inline int const bitstamp_websocket_port = 443;

  public:
  // ---------------------------------------
  // singleton access to network/testnet
  // ---------------------------------------
  static std::shared_ptr<abstract_exchange> get_instance()
  {
    static std::shared_ptr<abstract_exchange> bitstamp_ptr = nullptr;
    if (bitstamp_ptr == nullptr) bitstamp_ptr = std::make_shared<bitstamp_network>();
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

  // ---------------------------------------
  static bool get_pass_authentication(bitstamp_account& account);

  // returns a temp vector of account pointers (references)
  // to be used with caution because adding a wallet will
  // invalidate the pointer references
  std::vector<basic_account*> wallets() override
  {
    std::vector<basic_account*> accts;
    for (auto& acct : accounts_) { accts.push_back(&acct); }
    return accts;
  }

  std::vector<bitstamp_account>& accounts() { return accounts_; }

  bitstamp_account& get_account_by_name(std::string_view name);

  // Is sending this currency to the destination abstract_exchange supported
  bool can_send(currency_code const& c, abstract_exchange* dest) override;

  int get_decimals() override { return 2; }

  // ---------------------------------------
  // bitstamp wallets can be used for these trade algorithms
  // ---------------------------------------
  trade_action_list supported_trade_actions() override
  {
    return trade_action_list{
        supported_trade_actions::currency_exchange,
        supported_trade_actions::arbitrage_2way,
        supported_trade_actions::market_maker,
    };
  }

  // ---------------------------------------
  // return the order book for this abstract_exchange
  bitstamp_order_book const& get_orderbook(currency_pair const& cp) const;

  // ---------------------------------------
  // init connections/websockets etc
  bool subscribe_live_trades(currency_pair const& cp, bool enable);
  bool subscribe_order_book(currency_pair const& cp, bool enable);
  bool subscribe_my_trades(currency_pair const& cp, bool enable);
  bool subscribe_my_orders(currency_pair const& cp, bool enable);

  stream_set websocket_streams() override
  {
    return {
        ticker::streams::my_orders,      // private orders
        ticker::streams::my_trades,      // private trades
        ticker::streams::live_trades,    // all trades
        ticker::streams::order_book,     // all orders
        ticker::streams::price_data,     // ticker price feeds
    };
  }

  // connect to a single stream
  bool stream_subscribe(currency_pair const& cp, ticker::streams const stream, bool enabled,
      factory_function f) override;

  // connect to (multiple) streams
  //  bool websocket_connect(net::contexts& io_contexts, stream_set const&
  //  streams) override; bool websocket_disconnect(net::contexts& io_contexts,
  //  stream_set const& streams) override;

  // shut down sockets/connections
  void shut_down() override;

  // ---------------------------------------
  any_void_sender read_transaction_logs(std::string ini_name);
  any_void_sender update_transaction_logs(std::string ini_name);

  // ---------------------------------------
  // https: get currency tickers available
  any_bytearray_sender request_tickers_available();

  // https: get new websocket token to subscribe to streams
  any_bytearray_sender request_websocket_token();

  // https: get account info/data
  any_bytearray_sender request_account_info(bitstamp_account const& acct);
  // get account info for all accounts
  any_void_sender request_all_account_infos();

  // https: get open order data
  any_bytearray_sender request_account_orders(bitstamp_account const& acct);
  // get orders for all accounts
  any_void_sender request_all_account_orders();

  // https: get token deposit/withdrawal type crypto transactions
  any_bytearray_sender request_crypto_transactions(bitstamp_account const& acct);
  // get all token transactions
  any_void_sender request_all_crypto_transactions();

  // https: get usertransactions
  any_bytearray_sender request_account_transactions(bitstamp_account const& acct);
  // get all token transactions
  any_void_sender request_all_account_transactions();

  // https: get usertransactions
  any_bytearray_sender request_market_transactions(
      bitstamp_account const& acct, currency_pair const&);
  // get all token transactions
  any_void_sender request_all_market_transactions();

  // https: place a limit order
  any_bytearray_sender request_limit_order(bitstamp_account const& acct, trade_data const& t);
  // https: place an order cancel
  any_bytearray_sender request_cancel_order(trade_data const& t) override;

  // process account info response
  void handle_account_info(bitstamp_account& acct, std::string_view);
  void handle_websocket_token(std::string_view);
  void handle_tickers_available(std::string_view);

  // ---------------------------------------
  // process open order data response
  void handle_open_orders(bitstamp_account& acct, std::string_view);
  void process_order(bitstamp_account& acct, nlohmann::json& jdata, std::string_view event);

  // ---------------------------------------
  // make a payment/transfer from bitstamp
  bool make_payment(currency_amount const& c, basic_account* src, basic_account* dest) override;

  // ---------------------------------------
  // place a buy/sell order
  void place_buy_sell_orders(basic_account* acct, std::vector<trade_data> const& trades) override;
  void handle_buy_sell_order(std::string_view data);

  // ----------------------------------------------------------------------------
  net::http::client_ptr signed_request(
      bitstamp_account const& acct, std::string const& url_path, std::string const& url_query);

  // ----------------------------------------------------------------------------
  // OHLC candlestick updating
  // ----------------------------------------------------------------------------
  // triggers an update for all subscribed tickers
  // typically called once per minute by the application to update data regularly
  void update_ohlc_datasets();

  // triggers an update for a single ticker
  void update_ohlc_data(currency_pair cp, ticker::data data);

  // http : generate a request for candlestick data for a single ticker
  any_bytearray_sender request_new_ohlc_data(currency_pair cp, uint64_t start_t, uint64_t samples);

  // handler for an http request containing new data,
  // returns true unless there is a problemm that should stopfurther updates
  bool handle_new_ohlc_data(ticker::data, std::string_view);
  //
  any_bytearray_sender request_price_history(currency_pair cp);
  std::uint64_t handle_price_history(std::string_view data);

  // function called from websocket subscription to live trade data
  static void new_live_trade_data_q(bitstamp_network*, currency_pair cp, QString const);

  // function called from websocket subscription to live orderbook data
  static void new_orderbook_data_q(bitstamp_network*, currency_pair const cp, QString const);

  ticker::transaction_fees get_fees(currency_pair const& cp) override;

  void custom_functions(basic_account* /*acct*/) override {};

  stream_set ticker_subscribe(currency_pair const& cp) override;

  currency_pair split_token_string(std::string utoken) const;

  signals:

  // when the gui needs to be updated with new data/currencies
  void wallet_changed(ledger_wallet*);

  // when the gui needs to replot chart data
  void ohlc_data_changed(ledger_wallet*);

  // trigger this to restart the timer from a Qt thread
  void restart_candlestick_timer();

  public slots:
  void candlestick_timer_event();
};
