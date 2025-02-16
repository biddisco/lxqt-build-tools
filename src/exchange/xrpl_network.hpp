#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
//
#include <QObject>
//
#include <exec/any_sender_of.hpp>
#include <exec/async_scope.hpp>
#include <stdexec/execution.hpp>
//
#include "data/order_book.hpp"
#include "exchange/abstract_exchange.hpp"
#include "exchange/account.hpp"
#include "exchange/order_book_xrpl.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "network/qwebsocket_session.hpp"
#include "senders/sender_defs.hpp"

// ----------------------------------------------------------------------------
// #define GROX_USE_LOCAL_SERVER
#define GROX_USE_RIPPLE_MAINNET_SERVER

// ----------------------------------------------------------------------------
class xrpl_network : public abstract_exchange
{
  Q_OBJECT

  private:
  // websocket for order book trades
  std::shared_ptr<net::ws::qwebsocket_session> ws_orderbook;
  // websocket for account changes
  std::shared_ptr<net::ws::qwebsocket_session> ws_accounts;

  bool testnet_;
  std::vector<ledger_wallet> subscribed_wallets_;

  std::map<std::string, double> currency_fees_;

  // ---------------------------------------
  // MainNet : rippled server
  // ---------------------------------------
#if defined(GROX_USE_LOCAL_SERVER)
  // note that we use 6005 instead of 443 on local server to avoid
  // requiring sudo permissions to run rippled
  static inline std::string const ripple_websocket_address = "192.168.1.10";
  static inline int const ripple_websocket_port = 6005;

  static inline std::string const ripple_jsonrpc_address = "192.168.1.10";
  static inline int const ripple_jsonrpc_port = 51234;

#elif defined(GROX_USE_RIPPLE_MAINNET_SERVER)
  static inline std::string const ripple_websocket_address = "s1.ripple.com";
  static inline int const ripple_websocket_port = 443;

  static inline std::string const ripple_jsonrpc_address = "s1.ripple.com";
  static inline int const ripple_jsonrpc_port = 51234;

#else
  static inline std::string const ripple_websocket_address = "xrplcluster.com";
  static inline int const ripple_websocket_port = 443;

  static inline std::string const ripple_jsonrpc_address = "xrplcluster.com";
  static inline int const ripple_jsonrpc_port = 443;
#endif

  // ---------------------------------------
  // TestNet rippled server
  // ---------------------------------------
  // TestNet websocket
  static inline std::string const testnet_websocket_address = "s.altnet.rippletest.net";
  static inline int const testnet_websocket_port = 51233;

  // TestNet JSON RPC server
  static inline std::string const testnet_json_rpc_address = "s.altnet.rippletest.net";
  static inline int const testnet_json_rpc_port = 51234;

  private:
  // ---------------------------------------
  // singleton access to network/testnet
  // ---------------------------------------
  static std::shared_ptr<abstract_exchange> xrpl_instance()
  {
    static std::shared_ptr<abstract_exchange> xrpl_ptr = nullptr;
    if (xrpl_ptr == nullptr) xrpl_ptr = std::make_shared<xrpl_network>(false);
    return xrpl_ptr;
  }
  static std::shared_ptr<abstract_exchange> xrpltestnet_instance()
  {
    static std::shared_ptr<abstract_exchange> testnet_ptr = nullptr;
    if (testnet_ptr == nullptr) testnet_ptr = std::make_shared<xrpl_network>(true);
    return testnet_ptr;
  }

  public:
  static std::shared_ptr<abstract_exchange> get_instance(bool testnet)
  {
    if (testnet) return xrpltestnet_instance();
    return xrpl_instance();
  }
  static std::shared_ptr<xrpl_network> get_xrpl_instance(bool testnet)
  {
    return std::dynamic_pointer_cast<xrpl_network>(get_instance(testnet));
  }

  // ---------------------------------------
  // constructor/destructor
  // ---------------------------------------
  xrpl_network(bool testnet);
  ~xrpl_network() override;
  //

  void initialize() override;
  //
  bool testnet() const;
  //
  std::string websocket_address() const;
  int websocket_port() const;
  //
  std::string jsonrpc_address() const;
  int jsonrpc_port() const;
  //
  bool can_send(currency_code const& /*c*/, abstract_exchange* dest) override;
  //
  xrpl_order_book const& get_orderbook(currency_pair const& cp) const;
  //
  stream_set websocket_streams() override
  {
    return {ticker::streams::order_book, ticker::streams::account_changes};
  }

  stream_set ticker_subscribe(currency_pair const& cp) override;

  // connect to an individual stream
  bool stream_subscribe(currency_pair const& cp, ticker::streams const stream, bool enabled,
      factory_function f) override;

  // connect to (multiple) streams
  //  bool websocket_connect(net::contexts& io_contexts, stream_set const& streams) override;
  //  bool websocket_disconnect(net::contexts& io_contexts, stream_set const& streams) override;

  // shut down sockets/connections
  void shut_down() override;
  //
  bool subscribe_order_book(currency_pair const& cp, bool enable);
  //
  bool subscribe_accounts();
  //
  void add_wallet(ledger_wallet const& w);
  void clear_wallets() { subscribed_wallets_.clear(); }
  ledger_wallet* get_wallet_by_addr(std::string_view addr);
  ledger_wallet* get_wallet_by_name(std::string_view name);

  std::vector<basic_account*> wallets() override
  {
    std::vector<basic_account*> accts;
    for (auto& acct : subscribed_wallets_) { accts.push_back(&acct); }
    return accts;
  }

  // ----------------------------------------------------------------------------
  static void new_orderbook_data_q(xrpl_network* nw, currency_pair const cp, QString);
  // ----------------------------------------------------------------------------
  static void new_account_data_q(xrpl_network* nw, QString);

  // ----------------------------------------------------------------------------
  std::vector<currency_amount>::iterator get_currency(std::string_view addr, currency_code c);
  void update_XRP_balance(std::string_view addr, double oldb, double newb);
  void update_IOU_balance(std::string_view addr, currency_amount const& curr);

  any_bytearray_sender submit_signed_transaction(std::string&& signed_tx);

  // query account balance and info
  any_bytearray_sender get_account_info(std::string addr);
  void get_all_account_infos(exec::async_scope& scope);
  void handle_account_info(ledger_wallet& w, std::string_view data);

  // query account trustlines
  any_bytearray_sender get_account_lines(std::string addr);
  void get_all_account_lines(exec::async_scope& scope);
  void handle_account_lines(ledger_wallet& w, std::string_view data);

  // query open orders
  any_bytearray_sender get_account_offers(std::string addr);
  void get_all_account_offers(exec::async_scope& scope);
  void handle_account_offers(ledger_wallet& w, std::string_view data);

  bool make_payment(currency_amount const& c, basic_account* src, basic_account* dest) override;
  any_bytearray_sender request_cancel_order(trade_data const& t) override;

  // place a buy/sell order
  any_bytearray_sender request_limit_order(
      basic_account* acct, trade_data const& t, bool update_after);
  void place_buy_sell_orders(basic_account* acct, std::vector<trade_data> const& trades) override;

  ticker::transaction_fees get_fees(currency_pair const& cp) override;

  void trustline(
      basic_account* acct, std::string addr, std::string code, uint64_t limit, std::uint32_t flags);

  void custom_functions(basic_account* acct) override;

  void query_iou_fee(currency_code const& c1);

  signals:
  // Signals are emitted so that the Qt appication/GUI thread can perform
  // procesing operations that affect Qt/GUI managed items in a thread safe way

  // emitted when a single currency balance changes and GUI needs updating
  void update_currency_widget(currency_amount*);

  // emitted when a wallet is updated with new balances for multiple currencies
  void update_wallet_widget(ledger_wallet*);

  // emitted when a wallet is updated with new trade information
  void update_trade_widget(ledger_wallet*);
};
