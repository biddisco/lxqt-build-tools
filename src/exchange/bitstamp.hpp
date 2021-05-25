#pragma once

#include <QApplication>
#include <QString>
#include <QTimer>
//
#include <string>
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
//
#include "exchange.hpp"
#include "xrpl_network.hpp"

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

    bitstamp_network();
    ~bitstamp_network() override;

    //
    void connect(net::contexts &io_contexts) override;
    //
    void disconnect() override;

    void update_account_info();
    bool can_send(currency &c, exchange *dest) override;
    bool make_payment(currency &c, basic_account *src, basic_account *dest) override;
    //
    const bitstamp_order_book &get_orderbook() const;
    //
    void set_plot(OrderBookPlot *obp);

    // ----------------------------------------------------------------------------
    void account_request(std::string &&url_path, std::string &&url_query, request_callback &&cb);
    //
    void account_data(std::string&&);

    using fn_on_http_2 = std::function<void(OB::Belle::Client::Http_Ctx&, bool)>;

    void request_new_candlestick_data(uint64_t start_t, fn_on_http_2 fn);

    // function called from websocket subscription to live trade data
    static void new_trade_data(bitstamp_network*, std::string_view);

    // function called from websocket subscription to live orderbook data
    static void new_orderbook_data(bitstamp_network*, std::string_view);

signals:
    // Signals are emitted so that the Qt appication/GUI thread can perform
    // procesing operations that affect Qt/GUI managed items in a thread safe way

    // emitted when new orderbook data has been received and processed
    void orderbook_changed();

    // emitted when new
    void new_trade_data_ui(QString);

    // when the wallet widget needs to be updated with new data/currencies
    void widget_update();

public slots:
    void timer_event();

};
