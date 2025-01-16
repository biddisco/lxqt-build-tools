#pragma once

#include <memory>
//
#include <QWidget>
#include <QtCore>
//
#include "exchange/account.hpp"

namespace Ui {
  class wallet_widget;
}

class wallet_widget : public QWidget
{
  Q_OBJECT
  std::shared_ptr<exchange> network_;
  basic_account* account_;

  public:
  explicit wallet_widget(QWidget* parent = nullptr);
  ~wallet_widget();

  void set_data(ledger_wallet& w, int decimals = 6);
  void set_data(bitstamp_account& w);

  public slots:

  private:
  Ui::wallet_widget* ui;
};
