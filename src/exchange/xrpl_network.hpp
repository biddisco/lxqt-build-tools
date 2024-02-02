#pragma once

#include <string>
#include <vector>
//
#include <QObject>
//
#include <exec/any_sender_of.hpp>
#include <exec/async_scope.hpp>
#include <stdexec/execution.hpp>
//
#include "exchange/account.hpp"
#include "exchange/exchange.hpp"
#include "exchange/order_book.hpp"
#include "network/evp-encrypt.hpp"
#include "network/qhttp-request-client.hpp"
#include "network/qwebsocket_session.hpp"
#include "senders/sender_defs.hpp"
#include "widgets/currency_widget.hpp"

// ----------------------------------------------------------------------------
// #define GROX_USE_LOCAL_SERVER
#define GROX_USE_RIPPLE_MAINNET_SERVER

// ----------------------------------------------------------------------------
class xrpl_network : public exchange
{
  Q_OBJECT

  private:
  // websocket for order book trades
  std::shared_ptr<net::ws::qwebsocket_session> ws_orderbook;
  // websocket for account changes
  std::shared_ptr<net::ws::qwebsocket_session> ws_accounts;

  bool testnet_;
  xrpl_order_book* orderbook_;
  std::vector<ledger_wallet> subscribed_wallets_;

  std::map<std::string, double> currency_fees_;

  // ---------------------------------------
  // MainNet : rippled server
  // ---------------------------------------
#if defined(GROX_USE_LOCAL_SERVER)
  // note that we use 6005 instead of 443 on local server to avoid
  // requiring sudo permissions to run rippled
  static inline const std::string ripple_websocket_address = "192.168.1.10";
  static inline const int ripple_websocket_port = 6005;

  static inline const std::string ripple_jsonrpc_address = "192.168.1.10";
  static inline const int ripple_jsonrpc_port = 51234;

#elif defined(GROX_USE_RIPPLE_MAINNET_SERVER)
  static inline const std::string ripple_websocket_address = "s1.ripple.com";
  static inline const int ripple_websocket_port = 443;

  static inline const std::string ripple_jsonrpc_address = "s1.ripple.com";
  static inline const int ripple_jsonrpc_port = 51234;

#else
  static inline const std::string ripple_websocket_address = "xrplcluster.com";
  static inline const int ripple_websocket_port = 443;

  static inline const std::string ripple_jsonrpc_address = "xrplcluster.com";
  static inline const int ripple_jsonrpc_port = 443;
#endif

  // ---------------------------------------
  // TestNet rippled server
  // ---------------------------------------
  // TestNet websocket
  static inline const std::string testnet_websocket_address = "s.altnet.rippletest.net";
  static inline const int testnet_websocket_port = 51233;

  // TestNet JSON RPC server
  static inline const std::string testnet_json_rpc_address = "s.altnet.rippletest.net";
  static inline const int testnet_json_rpc_port = 51234;

  private:
  // ---------------------------------------
  // singleton access to network/testnet
  // ---------------------------------------
  static std::shared_ptr<exchange> xrpl_instance()
  {
    static std::shared_ptr<exchange> xrpl_ptr = nullptr;
    if (xrpl_ptr == nullptr)
      xrpl_ptr = std::make_shared<xrpl_network>(false);
    return xrpl_ptr;
  }
  static std::shared_ptr<exchange> xrpltestnet_instance()
  {
    static std::shared_ptr<exchange> testnet_ptr = nullptr;
    if (testnet_ptr == nullptr)
      testnet_ptr = std::make_shared<xrpl_network>(true);
    return testnet_ptr;
  }

  public:
  static std::shared_ptr<exchange> get_instance(bool testnet)
  {
    if (testnet)
      return xrpltestnet_instance();
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
  std::string_view name() override
  {
    return testnet() ? "XRPL Testnet" : "XRPL Mainnet";
  }
  //

  void initialize() override;

  void set_plot(OrderBookPlot* obp);
  //
  bool testnet() const;
  //
  std::string websocket_address() const;
  int websocket_port() const;
  //
  std::string jsonrpc_address() const;
  int jsonrpc_port() const;
  //
  bool can_send(currency const& /*c*/, exchange* dest) override;
  //
  xrpl_order_book const& get_orderbook(currency_pair const& cp) const;
  //
  streams_vector websocket_streams() override
  {
    return {network::streams::order_book, network::streams::accounts};
  }

  void ticker_subscribe(currency const& c1, currency const& c2) override;
  // connect to an individual stream
  bool stream_subscribe(
    currency_pair const& cp, network::streams const stream, bool enabled) override;

  // connect to (multiple) streams
  //  bool websocket_connect(net::contexts& io_contexts, streams_vector const& streams) override;
  //  bool websocket_disconnect(net::contexts& io_contexts, streams_vector const& streams) override;

  // shut down sockets/connections
  void shut_down() override;
  //
  bool subscribe_order_book(currency_pair const& cp, bool enable);
  //
  bool subscribe_accounts();
  //
  void add_wallet(ledger_wallet const& w);
  void clear_wallets()
  {
    subscribed_wallets_.clear();
  }
  ledger_wallet* get_wallet_by_addr(std::string_view addr);
  ledger_wallet* get_wallet_by_name(std::string_view name);

  std::vector<basic_account*> wallets() override
  {
    std::vector<basic_account*> accts;
    for (auto& acct : subscribed_wallets_)
    {
      accts.push_back(&acct);
    }
    return accts;
  }

  // ----------------------------------------------------------------------------
  static void new_orderbook_data(xrpl_network* nw, currency_pair const cp, QString);
  // ----------------------------------------------------------------------------
  static void new_account_data(xrpl_network* nw, QString);

  // ----------------------------------------------------------------------------
  std::vector<currency>::iterator get_currency(std::string_view addr, currency c);
  void update_XRP_balance(std::string_view addr, double oldb, double newb);
  void update_IOU_balance(std::string_view addr, currency const& curr);

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

  bool make_payment(currency const& c, basic_account* src, basic_account* dest) override;
  void cancel_order(trade_data const& t) override;

  // place a buy/sell order
  void place_limit_order(basic_account* acct, trade_data const& t, bool update_after);
  void place_buy_sell_orders(basic_account* acct, std::vector<trade_data> const& trades) override;

  double get_fee_percent(currency const& c1, currency const& c2) override;
  double get_fee_fixed(currency const& c1, currency const& c2) override;
  double get_transfer_fee(currency const& c1) override;

  void trustline(
    basic_account* acct, std::string addr, std::string code, uint64_t limit, std::uint32_t flags);

  void custom_functions(basic_account* acct) override;

  void query_iou_fee(currency_code const& c1);

  signals:
  // Signals are emitted so that the Qt appication/GUI thread can perform
  // procesing operations that affect Qt/GUI managed items in a thread safe way

  // emitted when new orderbook data has been received and processed
  void orderbook_changed();

  // emitted when a single currency balance changes and GUI needs updating
  void update_currency_widget(currency*);

  // emitted when a wallet is updated with new balances for multiple currencies
  void update_wallet_widget(ledger_wallet*);

  // emitted when a wallet is updated with new trade information
  void update_trade_widget(ledger_wallet*);
};
