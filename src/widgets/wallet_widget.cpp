#include "wallet_widget.hpp"
#include "currency_widget.hpp"
#include "ui_wallet_widget.h"
//
// ----------------------------------------------------------------------------
wallet_widget::wallet_widget(QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::wallet_widget)
{
  ui->setupUi(this);
  //
  connect(
      ui->net_funcs, &QToolButton::clicked, this,
      [this](bool /*checked*/) { network_->custom_functions(account_); }, Qt::QueuedConnection);
}

// ----------------------------------------------------------------------------
wallet_widget::~wallet_widget() { delete ui; }

// ----------------------------------------------------------------------------
void wallet_widget::set_data(ledger_wallet& w, int decimals)
{
  network_ = w.network_;
  account_ = &w;
  //
  ui->ledger_wallet->setTitle(w.name_.c_str());
  ui->address->setText(w.public_.c_str());
  ui->address->setTextInteractionFlags(Qt::TextSelectableByMouse);
  ui->tag->setText(QString(std::to_string(w.tag_).c_str()));
  ui->tag->setTextInteractionFlags(Qt::TextSelectableByMouse);
  //
  auto l = w.lock_currencies();
  for (auto& c : w.currencies_)
  {
    if (c.widget_ == nullptr)
    {
      c.widget_ = new currency_widget(decimals, this);
      c.widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      ui->currencies_layout->addWidget(c.widget_);
    }
    c.widget_->set_data(&c, &w, w.network_);
  }
  w.unlock_currencies(std::move(l));
  //
  update();
}

// ----------------------------------------------------------------------------
void wallet_widget::set_data(bitstamp_account& w) { this->set_data(w, 2); }

// ----------------------------------------------------------------------------
