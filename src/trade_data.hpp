#pragma once

// to pass structs as params we must declare metatypes to Qt
#include <QtCore>
//
#include <memory>
//
#include "currency.hpp"

class exchange;

// trade type : buy = 0, sell = 1
struct trade_data {
    std::shared_ptr<exchange> network_;
    int trade_type_;
    currency_type currency_pay_;
    currency_type currency_get_;
    double amount_pay_;
    double amount_get_;
    double value_;
    std::uint64_t id_;
    std::string datetime_;
};
