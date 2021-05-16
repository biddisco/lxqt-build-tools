#pragma once

#include <QObject>
//
#include "src/currency.hpp"
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"

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
};
