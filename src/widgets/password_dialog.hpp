#pragma once

#include <QDialog>
#include <QString>
//
#include <string>
#include <vector>
//
#include "exchange/account.hpp"
#include "ui_password_dialog.h"
//

class password_dialog : public QDialog
{
  Q_OBJECT
  std::vector<ledger_wallet> wallets_;

  public:
  password_dialog(bool simple);
  password_dialog(
    const std::array<std::string, 5>& strings, std::vector<ledger_wallet> const& wallets);
  ~password_dialog();

  // Exchange details
  QString getAPIUser();
  QString getAPIKey();
  QString getAPISecret();
  QString getAPIDestTag();
  QString getAPIXRPAddress();

  // Wallet details
  std::vector<ledger_wallet> const& get_wallets();
  //
  QString getPassword();
  //

  private slots:
  void enable_ok_button();
  void add_wallet();
  void remove_wallet();
  void refresh_gui(int index);

  private:
  Ui::password_dialog ui;
  bool simple_mode_;
};
