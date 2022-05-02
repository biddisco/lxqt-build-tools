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
#include "src/trade_data.hpp"

class basic_account;

namespace network {
    enum streams {
        trades = 0,
        order_book = 1,
        accounts = 2,
    };
}
using streams_vector = std::vector<network::streams>;

static std::string stream_text(network::streams stype) {
    switch (stype) {
    case network::streams::trades:     return "Trades";
    case network::streams::order_book: return "Order Book";
    case network::streams::accounts:   return "Account Changes";
    }
    return "Unknown";
}

// ----------------------------------------------------------------------------
class exchange : public QObject
{
    Q_OBJECT

public:
    using exchange_vector = std::vector<std::shared_ptr<exchange>>;
    using currency_pairlist = std::vector<std::pair<currency, currency>>;

    std::map<network::streams, bool> enabled_streams_;

    // obligatory virtual destructor
    virtual ~exchange() {}

    // ---------------------------------------
    // websocket/stream connection management
    // ---------------------------------------
    virtual streams_vector websocket_streams() = 0;

    virtual bool websocket_enabled(network::streams s)
    {
        if (enabled_streams_.find(s)!=enabled_streams_.end()) {
            return enabled_streams_[s];
        }
        return false;
    }

    virtual void websocket_enable(network::streams s, net::contexts &io_contexts, bool enable)
    {
        if (enable) {
            if (!websocket_enabled(s)) {
                enabled_streams_[s] = connect(io_contexts, {s});
            }
        }
        else {
            if (websocket_enabled(s)) {
                enabled_streams_[s] = !disconnect(io_contexts, {s});
            }
        }
    }

    virtual bool connect(net::contexts &io_contexts, streams_vector const &streams) = 0;
    virtual bool disconnect(net::contexts &io_contexts, streams_vector const &streams) = 0;
    //
    virtual void shut_down() = 0;

    // ---------------------------------------
    // currency management
    // ---------------------------------------
    virtual bool can_send(currency &c, exchange *dest) = 0;
    virtual bool make_payment(currency &c, basic_account *src, basic_account *dest) = 0;
    virtual std::string_view name() = 0;
    virtual void cancel_order(trade_data const &t) = 0;
    virtual currency_pairlist currency_pairs() = 0;
    virtual void place_buy_sell_orders(basic_account*, std::vector<trade_data> const &) = 0;
    virtual std::vector<basic_account*> wallets() = 0;

    // ---------------------------------------
    // fees
    // ---------------------------------------
    virtual double get_fee_percent(const currency_type &c1, const currency_type &c2) = 0;
    virtual double get_fee_fixed(const currency_type &c1, const currency_type &c2) = 0;
    virtual double get_transfer_fee(const currency &c1)  = 0;
    virtual void custom_functions(basic_account *acct) = 0;

signals:
    // emitted when a transaction might cause a change in data
    void transaction_event();
};
