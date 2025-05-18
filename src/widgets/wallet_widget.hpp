#pragma once

#include <functional>
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
  std::shared_ptr<abstract_exchange> network_;
  basic_account* account_;

  public:
  explicit wallet_widget(QWidget* parent = nullptr);
  ~wallet_widget();

  void add_algorithm(std::string name, std::function<void(void)> f);

  void set_data(ledger_wallet* w);

  public slots:

  private:
  Ui::wallet_widget* ui;
};
