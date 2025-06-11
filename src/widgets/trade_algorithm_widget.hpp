#pragma once

#include <memory>
//
#include <QDialog>
//
#include "exchange/abstract_exchange.hpp"

QDialog* create_trading_widget(abstract_exchange::exchange_vector exchange_list_,
    indicators::indicator_vector indicator_list = {});
QDialog* trade_widget_factory(std::shared_ptr<indicators::algorithm_base> alg,
    abstract_exchange::exchange_vector exchange_list_);
QDialog* dock_trading_widget(QDialog* algowidget, std::string name);

QDialog* create_trade_algorithm_widget(
    abstract_exchange::exchange_vector exchange_list_, indicators::algorithm_ptr alg);
