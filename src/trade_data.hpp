#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <memory>
//
#include "currency.hpp"

class exchange;

enum trade_type {
    buy = 0,
    sell,
    trade,
};

enum order_type {
    limit = 0,
    market,
    fill_or_kill,
};

// trade type : buy = 0, sell = 1
struct trade_data {
    std::shared_ptr<exchange> network_;
    trade_type trade_type_;
    currency_type taker_payc_;
    currency_type taker_getc_;
    double taker_pay_;
    double taker_get_;
    double exchange_rate_;
    double fee_;
    std::uint64_t id_;
    std::string datetime_;

    // if we are buying or selling xrp, then how many?
    double xrp_amount() const {
        // if taker gets xrp, we must be selling xrp
        if (taker_payc_==currency_type::xrp || taker_getc_==currency_type::xrp) {
            if (trade_type_ == trade_type::buy) {
                return taker_get_/exchange_rate_;
            }
            else {
                return taker_pay_;
            }
        }
        else {
            throw std::runtime_error("Not an xrp transaction");
        }
        return 0;
    }
};
