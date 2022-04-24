#pragma once

#include <QObject>
#include <QString>
//
#include <string>
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
//
#ifndef Q_MOC_RUN
// MOC chokes on keyword "signals" used by belle
# include "extern/belle/include/belle.hh"
#endif
//
#include "src/exchange/exchange.hpp"
#include "src/order_book.hpp"
#include "src/settings.hpp"
#include "src/widgets/currency_widget.hpp"

// ----------------------------------------------------------------------------
class xrpl_network : public exchange
{
    Q_OBJECT

private:
    // websocket for order book trades
    std::shared_ptr<net::ws::session> ws_orderbook;
    // websocket for account changes
    std::shared_ptr<net::ws::session> ws_accounts;

    bool testnet_;
    xrpl_order_book *orderbook_;
    std::vector<ledger_wallet> subscribed_wallets_;

    std::map<std::string, double> currency_fees_;

    // ---------------------------------------
    // MainNet : rippled server
    // ---------------------------------------
#if 1 || GROX_USE_RIPPLE_MAINNET_SERVER
    static inline const std::string ripple_mainnet_address = "s1.ripple.com";
#else
    static inline const std::string ripple_mainnet_address = "xrplcluster.com";
#endif
    static inline const int ripple_mainnet_port = 443;

    // MainNet : JSON RPC server
    static inline const std::string ripple_jsonrpc_address = "s1.ripple.com";
    static inline const int ripple_jsonrpc_port = 51234;

    // MainNet : data api
    static inline const std::string ripple_dataapi_address = "data.ripple.com";
    static inline const int ripple_dataapi_port = 443;

    // ---------------------------------------
    // TestNet rippled server
    // ---------------------------------------
    static inline const std::string ripple_testnet_address = "s.altnet.rippletest.net";
    static inline const int ripple_testnet_port = 51233;

    // TestNet JSON RPC server
    static inline const std::string ripple_jsonrpc_testaddr = "s.altnet.rippletest.net";
    static inline const int ripple_jsonrpc_testport = 51234;

    // TestNet data api
    static inline const std::string ripple_testapi_address = "testnet.data.api.ripple.com";
    static inline const int ripple_testapi_port = 443;

private:
    // ---------------------------------------
    // singleton access to network/testnet
    // ---------------------------------------
    static std::shared_ptr<exchange> xrpl_instance() {
        static std::shared_ptr<exchange> xrpl_ptr = nullptr;
        if (xrpl_ptr==nullptr)
            xrpl_ptr = std::make_shared<xrpl_network>(false);
        return xrpl_ptr;
    }
    static std::shared_ptr<exchange> xrpltestnet_instance() {
        static std::shared_ptr<exchange> testnet_ptr = nullptr;
        if (testnet_ptr==nullptr)
            testnet_ptr = std::make_shared<xrpl_network>(true);
        return testnet_ptr;
    }

public:
    static std::shared_ptr<exchange> get_instance(bool testnet) {
        if (testnet)
            return xrpltestnet_instance();
        return xrpl_instance();
    }
    static std::shared_ptr<xrpl_network> get_xrpl_instance(bool testnet) {
        return std::dynamic_pointer_cast<xrpl_network>(get_instance(testnet));
    }

    // ---------------------------------------
    // constructor/destructor
    // ---------------------------------------
    xrpl_network(bool testnet);
    ~xrpl_network() override;
    //
    std::string_view name() override { return "XRPL"; }
    //
    void set_plot(OrderBookPlot * obp);
    //
    bool testnet() const;
    //
    std::string network_address() const;
    std::string jsonrpc_address() const;
    std::string dataapi_address() const;
    int network_port() const;
    int jsonrpc_port() const;
    int dataapi_port() const;
    //
    bool can_send(currency &/*c*/, exchange *dest) override;
    //
    const xrpl_order_book &get_orderbook() const;
    //
    streams_vector websocket_streams() override {
        return {
            network::streams::order_book,
            network::streams::accounts
        };
    }

    // connect to (multiple) streams
    bool connect(net::contexts &io_contexts, streams_vector const &streams) override;
    bool disconnect(net::contexts &io_contexts, streams_vector const &streams) override;

    // shut down sockets/connections
    void shut_down() override;
    //
    bool subscribe_orderbook(net::contexts &io_contexts);
    //
    bool subscribe_accounts(net::contexts &io_contexts);
    //
    void add_wallet(const ledger_wallet &w);
    void clear_wallets() { subscribed_wallets_.clear(); }
    ledger_wallet *get_wallet_by_addr(std::string_view addr);
    ledger_wallet *get_wallet_by_name(std::string_view name);

    std::vector<basic_account*> wallets() override
    {
        std::vector<basic_account*> accts;
        for (auto &acct : subscribed_wallets_) {
            accts.push_back(&acct);
        }
        return accts;
    }

    // ----------------------------------------------------------------------------
    static void new_orderbook_data(xrpl_network* nw, std::string_view);
    // ----------------------------------------------------------------------------
    static void new_account_data(xrpl_network* nw, std::string_view);

    // ----------------------------------------------------------------------------
    std::vector<currency>::iterator get_currency(std::string_view addr, currency_type t);
    void update_XRP_balance(std::string_view addr, double oldb, double newb);
    void update_IOU_balance(std::string_view addr, const currency &curr);

    // Send query to Data API and get balances for all tracked wallets
    void get_all_account_balances();
    void handle_account_balance(ledger_wallet &w, std::string&& data);

    // Send query to Data API and get info for address
    using fn_on_http = std::function<void(OB::Belle::Client::Http_Ctx&)>;
    void get_account_info(std::string addr, fn_on_http on_http);

    // Send query to Data API and get info for all tracked wallets
    void get_all_account_infos();
    void handle_account_info(ledger_wallet &w, std::string&& data);

    // Send query to Data API and get all open orders for tracked wallets
    void get_all_account_orders();
    void handle_account_orders(ledger_wallet &w, std::string&& data);

    bool make_payment(currency &c, basic_account *src, basic_account *dest) override;
    void cancel_order(trade_data const &t) override;


    currency_pairlist currency_pairs() override;

    // place a buy/sell order
    void place_limit_order(basic_account *acct, trade_data const &t, bool update_after);
    void place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades) override;

    void submit_signed_transaction(std::string &&signed_tx);

    double get_fee_percent(const currency_type &c1, const currency_type &c2) override;
    double get_fee_fixed(const currency_type &c1, const currency_type &c2) override;
    double get_transfer_fee(const currency &c1) override;

    void trustline(basic_account *acct, std::string addr, std::string code, uint64_t limit, std::uint32_t flags);

    void custom_functions(basic_account *acct) override;

    void query_iou_fee(const issued_currency &c1);

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
