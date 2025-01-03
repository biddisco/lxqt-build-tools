#pragma once

#include <QDialog>
//
#include "exchange/exchange.hpp"

std::shared_ptr<QDialog> create_trading_widget(exchange::exchange_vector exchange_list_);
