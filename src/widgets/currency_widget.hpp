#pragma once

#include <QWidget>
//
#include "currency/currency.hpp"

class exchange;
class basic_account;

namespace Ui {
  class currency_widget;
}

class currency_widget : public QWidget
{
  Q_OBJECT

  private:
  Ui::currency_widget* ui;
  int decimals_;
  currency_amount currency_;
  basic_account* account_;
  std::shared_ptr<exchange> network_;
  double amount_;

  public:
  currency_widget(int decimals, QWidget* parent = nullptr);
  ~currency_widget();

  void set_data(currency_amount const* c, basic_account* acct = nullptr,
      std::shared_ptr<exchange> network = nullptr);
  void buy_sell_status();

  public slots:
  // ----------------------------------
  void transfer_setup_xrp(double);
  //
  void q1x_clicked();
  void q2x_clicked();
  void q3x_clicked();
  void q4x_clicked();

  void show_hide();
  double get_amount();
  void execute_payment();
  void execute_trade();
};
