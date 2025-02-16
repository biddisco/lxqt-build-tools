#include <cstdint>
#include <iostream>
#include <string>
//
#include <xrpl/protocol/TxFlags.h>
//
#include "currency/currency.hpp"
#include "exchange/abstract_exchange.hpp"
#include "exchange/xrpl_network.hpp"
#include "widgets/check_trades_dialog.hpp"
#include "widgets/trade_widget.hpp"
#include "widgets/xrp_functions.hpp"
//
#include "ui_xrp_functions.h"
// ----------------------------------------------------------------------------
xrp_functions::xrp_functions(xrpl_network* network, basic_account* account, QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::xrp_functions)
  , currency_(currency_code{"", ""})
  , account_(account)
  , network_(network)
{
  ui->setupUi(this);
  for (auto const& t : currencies::trustlines)
  {
    std::string temp = hex_to_currency(t.code_) + " " + t.issuer_;
    ui->trustlines->addItem(QString(temp.c_str()));
  }
  ui->limit->setValidator(new QDoubleValidator(0, 100E9, 1, this));
  //
  connect(ui->trustlines, &QComboBox::currentTextChanged, this, [this](QString text) {
    std::string txt = text.toStdString();
    auto space = txt.find(' ');
    ui->address->setText(txt.substr(space + 1, txt.back()).c_str());
    ui->code->setText(txt.substr(0, space).c_str());
  });
  //
  connect(ui->exec_trustline, &QToolButton::clicked, this, [this]() {
    std::string addr = ui->address->text().toStdString();
    std::string code = ui->code->text().toStdString();
    uint64_t limit = ui->limit->text().toULongLong();
    std::uint32_t flags = ripple::tfSetNoRipple;
    if (ui->rippling->isChecked()) flags = ripple::tfClearNoRipple;
    std::cout << "FIX: QToolButton::clicked " << addr << " " << code << " " << limit << std::endl;
    network_->trustline(account_, addr, code, limit, flags);
  });
}

// ----------------------------------------------------------------------------
xrp_functions::~xrp_functions() { delete ui; }

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
