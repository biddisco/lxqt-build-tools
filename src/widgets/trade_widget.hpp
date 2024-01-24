#pragma once

#include <QWidget>
#include <string_view>

#include "currency/currency.hpp"
#include "currency/trade_data.hpp"
#include "exchange/exchange.hpp"

namespace Ui {
  class trade_widget;
}

class trade_widget : public QWidget
{
  Q_OBJECT

  trade_data trade_;

  public:
  explicit trade_widget(QWidget* parent = nullptr);
  explicit trade_widget(std::string_view data, QWidget* parent = nullptr);
  ~trade_widget();

  void connect_events();
  void set_data(trade_data const& t);
  void cancel_order();

  private:
  Ui::trade_widget* ui;
};
