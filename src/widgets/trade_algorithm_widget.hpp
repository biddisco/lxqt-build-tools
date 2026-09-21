#pragma once

#include <memory>
//
#include <QDialog>
//
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_registry.hpp"

QDialog* trade_widget_factory(
    indicators::shared_algorithm alg, abstract_exchange::exchange_vector exchange_list_);
QDialog* dock_trading_widget(QDialog* algowidget, std::string name);
