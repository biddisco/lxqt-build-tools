#include <QApplication>
#include <QString>
//
#include <string>
//
#include "src/internet/https-async.hpp"
#include "src/internet/websocket-ssl.hpp"
#include "src/internet/evp-encrypt.hpp"
//
#include "bitstamp.hpp"
//
// ----------------------------------------------------------------------------
bool bitstamp_network::can_send(currency &c, exchange *dest)
{
    if (dynamic_cast<xrpl_network*>(dest)) {
        if (c.type_==currency_type::xrp || c.type_==currency_type::usd_bitstamp || c.type_==currency_type::eur_bitstamp)
            return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::make_payment(currency &c, basic_account *src, basic_account *dest)
{
    std::cout << "Bitstamp payment sent" << std::endl;
}
