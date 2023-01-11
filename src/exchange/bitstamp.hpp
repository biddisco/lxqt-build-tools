#pragma once

#include <QString>
//
#include <chrono>
#include <memory>
#include <string>
#include <vector>
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
    // websocket for private trades
    std::shared_ptr<net::ws::session> ws_mytrades;
    // websocket for private orders
    std::shared_ptr<net::ws::session> ws_myorders;
    // websocket for public trade feed
    std::shared_ptr<net::ws::session> ws_trades;
    // websocket for public bid/ask order book
    std::shared_ptr<net::ws::session> ws_bidask;

    // websocket token / user id valid for N seconds
    std::string websocket_token_;
    // websocket token userid
    std::string websocket_user_id_;
    // token expiry time
    std::chrono::time_point<std::chrono::steady_clock> token_expiry_;

    // orderbook from bitstamp
    bitstamp_order_book *orderbook_;

    // usually only one present, but allow for more
    std::vector<bitstamp_account> accounts_;

    // map of fees for trading of currency pairs
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

    void initialize() override;

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
    bool add_currency_pair(std::string_view c1, std::string_view c2) override;

    // Is sending this currency to the destination exchange supported
    bool can_send(currency &c, exchange *dest) override;

    // ---------------------------------------
    // return the order book for this exchange
    const bitstamp_order_book &get_orderbook() const;

    // set the plot object for this exchange's orderbook
    void set_plot(OrderBookPlot *obp);

    // ---------------------------------------
    // init connections/websockets etc
    bool subscribe_live_trades(net::contexts &io_contexts);
    bool subscribe_order_book(net::contexts &io_contexts);
    bool subscribe_my_trades(net::contexts &io_contexts);
    bool subscribe_my_orders(net::contexts &io_contexts);
    bool unsubscribe_my_trades();
    bool unsubscribe_my_orders();

    streams_vector websocket_streams() override {
        return {
            network::streams::my_orders,
            network::streams::my_trades,
            network::streams::trades,
            network::streams::order_book
        };
    }
    // connect to (multiple) streams
    bool connect(net::contexts &io_contexts, streams_vector const &streams) override;
    bool disconnect(net::contexts &io_contexts, streams_vector const &streams) override;

    // shut down sockets/connections
    void shut_down() override;

    // ---------------------------------------
    // http: fetch account info/data
    void get_account_info();
    void get_websocket_token();

    // process account info response
    void handle_account_info(std::string&&);
    void handle_websockets_token(std::string&&);

    // ---------------------------------------
    // http: fetch open order data
    void get_open_orders();
    // process open order data response
    void handle_open_orders(std::string&&);
    void process_order(nlohmann::json &jdata, std::string_view event);

    // ---------------------------------------
    // make a payment/transfer from bitstamp
    bool make_payment(currency &c, basic_account *src, basic_account *dest) override;

    // ---------------------------------------
    // place a buy/sell order
    void place_limit_order(trade_data const &t, bool update_after);
    void place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades) override;
    void cancel_order(trade_data const &t) override;

    // ----------------------------------------------------------------------------
    void account_request(std::string &&url_path, std::string &&url_query, request_callback &&cb);
    //

    using fn_on_http_2 = std::function<void(OB::Belle::Client::Http_Ctx&, bool)>;

    bool request_new_candlestick_data(uint64_t start_t, fn_on_http_2 fn);

    // function called from websocket subscription to live trade data
    static void new_trade_data(bitstamp_network*, std::string_view);

    // function called from websocket subscription to live orderbook data
    static void new_orderbook_data(bitstamp_network*, std::string_view);

    double get_fee_percent(const currency_type &c1, const currency_type &c2) override;
    double get_fee_fixed(const currency_type &c1, const currency_type &c2) override;
    double get_transfer_fee(const currency &/*c1*/)  override { return 0; }

    void custom_functions(basic_account */*acct*/) override {};

    void request_tickers_available();
    void receive_tickers_available(std::string &&data);

    void subscribe_currency_pair(std::string_view p1) override;

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

};
