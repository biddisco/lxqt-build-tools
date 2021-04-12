#pragma once

#include <QObject>
//
#include "../currency.hpp"

class basic_account;

// ----------------------------------------------------------------------------
class exchange : public QObject
{
    Q_OBJECT

public:
    virtual ~exchange() {}
    virtual void connect() = 0;
    virtual bool can_send(currency &c, exchange *dest) = 0;
    virtual bool make_payment(currency &c, basic_account *src, basic_account *dest) = 0;
};
