#pragma once

#include <QObject>
//
#include "src/currency.hpp"
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/trade_data.hpp"

class basic_account;

// ----------------------------------------------------------------------------
class exchange : public QObject
{
    Q_OBJECT

public:
    virtual ~exchange() {}
    virtual void connect(net::contexts &io_contexts) = 0;
    virtual void disconnect() = 0;
    virtual bool can_send(currency &c, exchange *dest) = 0;
    virtual bool make_payment(currency &c, basic_account *src, basic_account *dest) = 0;
    virtual std::string_view name() = 0;
    virtual void cancel_order(trade_data const &t) = 0;
    virtual std::vector<std::pair<currency_type, currency_type>> currency_pairs() = 0;
    virtual void place_buy_sell_orders(std::vector<trade_data> const &trades) = 0;

signals:
    // emitted when a transaction might cause a balance change
    void transaction_event();
};
