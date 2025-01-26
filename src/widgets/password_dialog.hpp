#pragma once

#include <array>
#include <string>
#include <vector>
//
#include <QDialog>
#include <QString>
//
#include "exchange/account.hpp"
#include "exchange/bitstamp.hpp"
#include "exchange/xrpl_network.hpp"
//
namespace Ui {
  class password_dialog;
}

class password_dialog : public QDialog
{
  Q_OBJECT
  std::vector<ledger_wallet> wallets_;

  public:
  password_dialog(bool simple);
  password_dialog(
      std::vector<bitstamp_account> const& bitstamp, std::vector<ledger_wallet> const& wallets);
  ~password_dialog();

  // Bitstamp exchange accounts
  std::vector<bitstamp_account> const& geBitstampwallets();

  QString getBitstampUser(int index);
  QString getBitstampKey(int index);
  QString getBitstampSecret(int index);
  QString getBitstampDestTag(int index);
  QString getBitstampXRPAddress(int index);

  // XRP ledger Wallet details
  std::vector<ledger_wallet> const& getXRPwallets();
  //
  QString getPassword();
  //

  private slots:
  void enable_ok_button();
  void add_wallet();
  void remove_wallet();
  void refresh_gui(int index);

  private:
  Ui::password_dialog* ui;
  bool simple_mode_;
};
