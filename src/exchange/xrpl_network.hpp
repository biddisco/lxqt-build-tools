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
#include "exchange.hpp"
#include "../order_book.hpp"
#include "../settings.hpp"
#include "../currency_widget.hpp"

// ----------------------------------------------------------------------------
class xrpl_network : public exchange
{
    Q_OBJECT

private:
    // websocket for order book trades
    std::shared_ptr<net::ws::session> ws_orderbook;
    // websocket for account changes
    std::shared_ptr<net::ws::session> ws_accounts;
    // https client for xrpl data API
    OB::Belle::Client belle_https_ripple;
    // https client for xrpl network submissions
    OB::Belle::Client belle_jsonrpc_network;

    bool testnet_;
    xrpl_order_book *orderbook_;
    std::vector<ledger_wallet> subscribed_wallets_;

    // ---------------------------------------
    // MainNet : rippled server
    // ---------------------------------------
    static inline const std::string ripple_mainnet_address = "s1.ripple.com";
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

public:
    // ---------------------------------------
    // singleton access to network/testnet
    // ---------------------------------------
    static std::shared_ptr<exchange> get_xrpl_instance() {
        static std::shared_ptr<exchange> xrpl_ptr = nullptr;
        if (xrpl_ptr==nullptr)
            xrpl_ptr = std::make_shared<xrpl_network>(false);
        return xrpl_ptr;
    }
    static std::shared_ptr<exchange> get_xrpltestnet_instance() {
        static std::shared_ptr<exchange> testnet_ptr = nullptr;
        if (testnet_ptr==nullptr)
            testnet_ptr = std::make_shared<xrpl_network>(true);
        return testnet_ptr;
    }
    static std::shared_ptr<exchange> get_instance(bool testnet) {
        if (testnet)
            return get_xrpltestnet_instance();
        return get_xrpl_instance();
    }

    // ---------------------------------------
    // constructor/destructor
    // ---------------------------------------
    xrpl_network(bool testnet);
    ~xrpl_network() override;
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
    void connect(net::contexts &io_contexts) override;
    //
    void disconnect() override;
    //
    void subscribe_orderbook(net::contexts &io_contexts);
    //
    void subscribe_accounts(net::contexts &io_contexts);
    //
    void add_wallet(const ledger_wallet &w);

    // ----------------------------------------------------------------------------
    static void new_order_data(xrpl_network* nw, std::string_view);
    // ----------------------------------------------------------------------------
    static void new_account_data(xrpl_network* nw, std::string_view);

    // ----------------------------------------------------------------------------
    void update_balance(std::string_view addr, double oldb, double newb);

    // Send query to Data API and get balances for all tracked wallets
    void get_all_account_balances();
    void handle_account_balance(ledger_wallet &w, std::string&& data);

    // Send query to Data API and get info for all tracked wallets
    void get_all_account_infos();
    void handle_account_info(ledger_wallet &w, std::string&& data);

    bool make_payment(currency &c, basic_account *src, basic_account *dest) override;

signals:
    // Signals are emitted so that the Qt appication/GUI thread can perform
    // procesing operations that affect Qt/GUI managed items in a thread safe way

    // emitted when a currency balance changes
    void update_currency_widget(currency*);
    // emitted when a wallet is updated with new balances for multiple currencies
    void update_wallet_widget(ledger_wallet*);
    // emitted when new order book data is ready
    void new_order_book_data(QString);
};
