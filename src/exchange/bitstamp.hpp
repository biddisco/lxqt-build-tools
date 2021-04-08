#pragma once

#include <QApplication>
#include <QString>
//
#include <string>
//
#include "src/internet/https-async.hpp"
#include "src/internet/websocket-ssl.hpp"
#include "src/internet/evp-encrypt.hpp"
//
#include "exchange.hpp"
#include "xrpl_network.hpp"

// ----------------------------------------------------------------------------
class bitstamp_network : public exchange
{
    Q_OBJECT

public:
    ~bitstamp_network() override {}
    bool can_send(currency &c, exchange *dest) override;
};
