#pragma once

#include <QDialog>
//
#include "exchange/exchange.hpp"

namespace Ui {
  class arbitrage_widget;
}

class arbitrage_widget : public QDialog
{
  Q_OBJECT

  private:
  Ui::arbitrage_widget* ui_;

  public:
  explicit arbitrage_widget(QWidget* parent)
    : QDialog(parent) {};

  ~arbitrage_widget() {};
};

std::shared_ptr<arbitrage_widget> create_arbitrage_widget(exchange::exchange_vector exchange_list_);
