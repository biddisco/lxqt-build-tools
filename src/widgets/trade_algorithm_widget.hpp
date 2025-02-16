#pragma once

#include <memory>
//
#include <QDialog>
//
#include "exchange/abstract_exchange.hpp"

std::shared_ptr<QDialog> create_trading_widget(abstract_exchange::exchange_vector exchange_list_);
