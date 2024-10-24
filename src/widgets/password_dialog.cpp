#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>
#include <QString>
//
#include "exchange/xrpl_network.hpp"
#include "password_dialog.hpp"
#include "ui_password_dialog.h"

// ----------------------------------------------------------------------------
password_dialog::password_dialog(bool simple)
  : QDialog()
  , simple_mode_(simple)
  , ui(new Ui::password_dialog)
{
  ui->setupUi(this);
  ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
  setWindowTitle("Wallet/Exchange Details");
  connect(ui->api_user, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->api_key, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->api_secret, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->api_tag, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->api_address, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->xrp_nickname, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->xrp_public, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->xrp_private, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->password, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->confirm, SIGNAL(textChanged(QString)), this, SLOT(enable_ok_button()));
  connect(ui->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(ui->add_wallet, SIGNAL(clicked()), this, SLOT(add_wallet()));
  connect(ui->remove_wallet, SIGNAL(clicked()), this, SLOT(remove_wallet()));
  connect(ui->wallets_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(refresh_gui(int)));

  if (simple_mode_)
  {
    ui->box_1->setVisible(false);
    ui->box_2->setVisible(false);
    ui->confirm->setVisible(false);
    ui->c_label->setVisible(false);
    adjustSize();
    resize(minimumSizeHint());
  }
}

password_dialog::password_dialog(
    std::array<std::string, 5> const& strings, std::vector<ledger_wallet> const& wallets)
  : password_dialog(false)
{
  // exchange data
  ui->api_user->setText(QString(strings[0].c_str()));
  ui->api_key->setText(QString(strings[1].c_str()));
  ui->api_secret->setText(QString(strings[2].c_str()));
  ui->api_tag->setText(QString(strings[3].c_str()));
  ui->api_address->setText(QString(strings[4].c_str()));
  // wallets
  wallets_ = wallets;
  for (auto const& w : wallets_) { ui->wallets_combo->addItem(QString(w.name_.c_str())); }
  ui->xrp_nickname->setText(wallets_[0].name_.c_str());
  ui->xrp_public->setText(wallets_[0].public_.c_str());
  ui->xrp_private->setText(wallets_[0].private_.c_str());
}

// ----------------------------------------------------------------------------
password_dialog::~password_dialog() { delete ui; }

// ----------------------------------------------------------------------------
QString password_dialog::getPassword() { return ui->password->text(); }

// ----------------------------------------------------------------------------
// Exchange details
// ----------------------------------------------------------------------------
QString password_dialog::getAPIUser() { return ui->api_user->text(); }

QString password_dialog::getAPIKey() { return ui->api_key->text(); }

QString password_dialog::getAPISecret() { return ui->api_secret->text(); }

QString password_dialog::getAPIDestTag() { return ui->api_tag->text(); }

QString password_dialog::getAPIXRPAddress() { return ui->api_address->text(); }

// ----------------------------------------------------------------------------
// Wallet details
// ----------------------------------------------------------------------------
std::vector<ledger_wallet> const& password_dialog::get_wallets() { return wallets_; }

void password_dialog::add_wallet()
{
  ledger_wallet w;
  w.name_ = ui->xrp_nickname->text().toStdString();
  w.public_ = ui->xrp_public->text().toStdString();
  w.private_ = ui->xrp_private->text().toStdString();
  w.testnet_ = ui->testnet->isChecked();
  w.network_ = xrpl_network::get_instance(w.testnet_);

  auto it = std::find_if(
      wallets_.begin(), wallets_.end(), [&](ledger_wallet& w2) { return w2.name_ == w.name_; });
  if (it == wallets_.end())
  {
    wallets_.push_back(w);
    ui->wallets_combo->addItem(QString(w.name_.c_str()));
  }
  else { *it = w; }
  ui->wallets_combo->setCurrentText(QString(w.name_.c_str()));
}

void password_dialog::remove_wallet()
{
  if (wallets_.size() > 1)
  {
    auto index = ui->wallets_combo->currentIndex();
    wallets_.erase(wallets_.begin() + index);
    ui->wallets_combo->removeItem(index);
  }
}

void password_dialog::refresh_gui(int index)
{
  ledger_wallet& w = wallets_[index];
  ui->xrp_nickname->setText(w.name_.c_str());
  ui->xrp_public->setText(w.public_.c_str());
  ui->xrp_private->setText(w.private_.c_str());
  ui->testnet->setChecked(w.testnet_);
}
// ----------------------------------------------------------------------------
// Validate
// ----------------------------------------------------------------------------
void password_dialog::enable_ok_button()
{
  if (!simple_mode_)
  {
    if (ui->api_user->text().isEmpty() || ui->api_key->text().isEmpty() ||
        ui->api_secret->text().isEmpty() || ui->api_tag->text().isEmpty() ||
        ui->api_address->text().isEmpty() || ui->password->text().isEmpty())
    {
      ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
      return;
    }
  }
  if (!simple_mode_ && ui->password->text() != ui->confirm->text())
  {
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    return;
  }
  //  ui->button_box->setEnabled(true);
  //  ui->button_box->button(QDialogButtonBox.Ok).setEnabled(true);
  ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
}
