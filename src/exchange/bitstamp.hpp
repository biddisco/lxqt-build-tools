#pragma once

#include <QString>
//
#include <string>
//
#ifndef Q_MOC_RUN
// MOC chokes on keyword "signals" used by belle
# include "extern/belle/include/belle.hh"
#endif
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/exchange/exchange.hpp"
#include "src/order_book.hpp"
#include "src/trade_data.hpp"
#include "src/settings.hpp"

// ----------------------------------------------------------------------------
class bitstamp_network : public exchange
{
    Q_OBJECT

private:
    // websocket for bitstamp trade feed
    std::shared_ptr<net::ws::session> ws_trades;
    // websocket for bitstamp bid/ask order book
    std::shared_ptr<net::ws::session> ws_bidask;

    // orderbook from bitstamp
    bitstamp_order_book *orderbook_;

    // usually only one present, but allow for more
    std::vector<bitstamp_account> accounts_;

    std::map<std::pair<std::string, std::string>, double> fee_map_;

public:
    //
    static inline const std::string bitstamp_https_address = "www.bitstamp.net";
    static inline const int bitstamp_https_port = 443;
    //
    static inline const std::string bitstamp_websocket_address = "ws.bitstamp.net";
    static inline const int bitstamp_websocket_port = 443;
    //
    using request_callback = std::function<void(std::string&&)>;

public:
    // ---------------------------------------
    // singleton access to network/testnet
    // ---------------------------------------
    static std::shared_ptr<exchange> get_instance() {
        static std::shared_ptr<exchange> bitstamp_ptr = nullptr;
        if (bitstamp_ptr==nullptr)
            bitstamp_ptr = std::make_shared<bitstamp_network>();
        return bitstamp_ptr;
    }

    static std::shared_ptr<bitstamp_network> get_bitstamp_instance() {
        return std::dynamic_pointer_cast<bitstamp_network>(get_instance());
    }

    // ---------------------------------------
    // construct/destruct
    // ---------------------------------------
    bitstamp_network();
    ~bitstamp_network() override;

    // returns a temp vector of account pointers (references)
    // to be used with caution because adding a wallet will
    // invalidate the pointer references
    std::vector<basic_account*> wallets() override
    {
        std::vector<basic_account*> accts;
        for (auto &acct : accounts_) {
            accts.push_back(&acct);
        }
        return accts;
    }

    bitstamp_account &account() {
        return accounts_[0];
    }

    // ---------------------------------------
    // network name
    std::string_view name() override { return "Bitstamp"; }

    // supported currency pairs
    virtual std::vector<std::pair<currency_type, currency_type>> currency_pairs() override;

    // Is sending this currency to the destination exchange supported
    bool can_send(currency &c, exchange *dest) override;

    // ---------------------------------------
    // return the order book for this exchange
    const bitstamp_order_book &get_orderbook() const;

    // set the plot object for this exchange's orderbook
    void set_plot(OrderBookPlot *obp);

    // ---------------------------------------
    // init connections/websockets etc
    void connect(net::contexts &io_contexts) override;
    // shut down sockets/connections
    void disconnect() override;

    // ---------------------------------------
    // http: fetch account info/data
    void get_account_info();
    // process account info response
    void handle_account_info(std::string&&);

    // ---------------------------------------
    // http: fetch open order data
    void get_open_orders();
    // process open order data response
    void handle_open_orders(std::string&&);


    bool make_payment(currency &c, basic_account *src, basic_account *dest) override;
    //

    // ---------------------------------------
    // place a buy/sell order
    void place_limit_order(trade_data const &t, bool update_after);
    void place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades) override;
    void cancel_order(trade_data const &t) override;

    // ----------------------------------------------------------------------------
    void account_request(std::string &&url_path, std::string &&url_query, request_callback &&cb);
    //

    using fn_on_http_2 = std::function<void(OB::Belle::Client::Http_Ctx&, bool)>;

    void request_new_candlestick_data(uint64_t start_t, fn_on_http_2 fn);

    // function called from websocket subscription to live trade data
    static void new_trade_data(bitstamp_network*, std::string_view);

    // function called from websocket subscription to live orderbook data
    static void new_orderbook_data(bitstamp_network*, std::string_view);

    double get_fee_percent(const currency_type &c1, const currency_type &c2) override;
    double get_fee_fixed(const currency_type &c1, const currency_type &c2) override;

    void custom_functions(basic_account *acct) override {};

signals:
    // Signals are emitted so that the Qt appication/GUI thread can perform
    // procesing operations that affect Qt/GUI managed items in a thread safe way

    // emitted when new orderbook data has been received and processed
    void orderbook_changed();

    // emitted when data for new trades is ready
    void new_trade_data_ui(live_trades);

    // when the wallet widget needs to be updated with new data/currencies
    void update_wallet_widget(bitstamp_account*);

public slots:
    void timer_event();

};
