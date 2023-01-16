#pragma once

#include <vector>
#include <string>
//
#include <QObject>
//
#include "src/currency.hpp"
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/data/ohlc_dataset_view.hpp"
#include "src/trade_data.hpp"

class basic_account;

namespace network {
    enum streams : int {
        my_trades,
        my_orders,
        trades,
        order_book,
        accounts,
    };
}
using streams_vector = std::vector<network::streams>;

static std::string stream_text(network::streams stype) {
    switch (stype) {
        case network::streams::my_trades:  return "My Trades";
        case network::streams::my_orders:  return "My Orders";
        case network::streams::trades:     return "Trades";
        case network::streams::order_book: return "Order Book";
        case network::streams::accounts:   return "Account Changes";
    }
    return "Unknown";
}

class price_chart_widget;

struct ticker_data {
    std::shared_ptr<ohlc_dataset_view> view_;
    price_chart_widget *chart_widget_;
};

// ----------------------------------------------------------------------------
class exchange : public QObject, public std::enable_shared_from_this<exchange>
{
    Q_OBJECT

public:
    using exchange_vector = std::vector<std::shared_ptr<exchange>>;
    using exchange_map = std::map<currency_pair, ticker_data>;

    // websocket streams subscribed to
    std::map<network::streams, bool> enabled_streams_;

    // ticker pairs available
    currency_pairlist tickers_available_;

    // ticker pairs subscribed to
    exchange_map tickers_subscribed_;

    // timer for data updates
    QTimer *timer_;

    // obligatory virtual destructor
    virtual ~exchange() {}

    // ---------------------------------------
    // concreate exchange instantiations must override the initialization
    // ---------------------------------------
    virtual void initialize() = 0;

    // ---------------------------------------
    // websocket/stream connection management
    // ---------------------------------------
    virtual streams_vector websocket_streams() = 0;

    virtual bool websocket_enabled(network::streams s);
    virtual void websocket_enable(network::streams s, net::contexts &io_contexts, bool enable);
    //
    virtual bool websocket_connect(net::contexts &io_contexts, streams_vector const &streams) = 0;
    virtual bool websocket_disconnect(net::contexts &io_contexts, streams_vector const &streams) = 0;
    //
    virtual void shut_down() = 0;

    // ---------------------------------------
    // currency management
    // ---------------------------------------
    virtual bool can_send(currency &c, exchange *dest) = 0;
    virtual bool make_payment(currency &c, basic_account *src, basic_account *dest) = 0;
    virtual std::string_view name() = 0;
    virtual void cancel_order(trade_data const &t) = 0;
    virtual void place_buy_sell_orders(basic_account*, std::vector<trade_data> const &) = 0;
    virtual std::vector<basic_account*> wallets() = 0;

    // ---------------------------------------
    // setup / query tickers
    // ---------------------------------------
    virtual bool add_currency_pair(std::string_view p1, std::string_view p2) = 0;
    virtual const currency_pairlist & get_currency_pairs();

    // ---------------------------------------
    // subscription to tickers
    // ---------------------------------------
    // query which tickers are subscribed
    virtual bool ticker_subscribed(const currency &c1, const currency &c2);
    virtual bool ticker_subscribed(std::string_view p1, std::string_view p2);
    // subscribe to a ticker
    virtual void ticker_subscribe(const currency &c1, const currency &c2);
    virtual void ticker_subscribe(std::string_view p1, std::string_view p2);
    // return list of subscribed tickers
    const exchange_map & tickers_subscribed();

    // ---------------------------------------
    // fees
    // ---------------------------------------
    virtual double get_fee_percent(const currency_type &c1, const currency_type &c2) = 0;
    virtual double get_fee_fixed(const currency_type &c1, const currency_type &c2) = 0;
    virtual double get_transfer_fee(const currency &c1)  = 0;
    virtual void custom_functions(basic_account *acct) = 0;

    virtual void start_timer() {}

signals:
    // emitted when a transaction might cause a change in data
    void transaction_event();
    void network_initialized(exchange *ex);
};
