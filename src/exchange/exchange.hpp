#pragma once

#include <QObject>
//
#include "../currency.hpp"

// ----------------------------------------------------------------------------
class exchange : public QObject
{
    Q_OBJECT

public:
    virtual ~exchange() {}
    virtual void connect() = 0;
    virtual bool can_send(currency &c, exchange *dest) = 0;
};
