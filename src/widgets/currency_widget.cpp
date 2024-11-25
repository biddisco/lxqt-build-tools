#include <string>
//
#include <QDialog>
#include <QMessageBox>
//
#include "config/config.hpp"
#include "exchange/account.hpp"
#include "exchange/exchange.hpp"
#include "widgets/check_trades_dialog.hpp"
#include "widgets/currency_widget.hpp"
#include "widgets/trade_widget.hpp"
//
#include "ui_currency_widget.h"

// ----------------------------------------------------------------------------
currency_widget::currency_widget(int decimals, QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::currency_widget)
  , decimals_(decimals)
  , currency_(currency_code{"", ""})
{
  ui->setupUi(this);
  ui->controls_amount->hide();
  ui->controls_pay->hide();
  ui->controls_trade->hide();
  ui->amount_edit->setValidator(new QDoubleValidator(0, 1E9, 6, this));
  ui->min_price->setDecimals(decimals_);
  ui->max_price->setDecimals(decimals_);
  //
  connect(ui->q1x, SIGNAL(clicked()), this, SLOT(q1x_clicked()));
  connect(ui->q2x, SIGNAL(clicked()), this, SLOT(q2x_clicked()));
  connect(ui->q3x, SIGNAL(clicked()), this, SLOT(q3x_clicked()));
  connect(ui->q4x, SIGNAL(clicked()), this, SLOT(q4x_clicked()));
  connect(ui->show, SIGNAL(clicked()), this, SLOT(show_hide()));
  connect(ui->exec_pay, SIGNAL(clicked()), this, SLOT(execute_payment()));
  connect(ui->exec_trade, SIGNAL(clicked()), this, SLOT(execute_trade()));
  connect(ui->amount_edit, SIGNAL(returnPressed()), this, SLOT(get_amount()));

  connect(ui->buy_sell_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
      [=](int /*index*/) { buy_sell_status(); });
}

// ----------------------------------------------------------------------------
currency_widget::~currency_widget() { delete ui; }

// ----------------------------------------------------------------------------
void currency_widget::set_data(
    currency const* c, basic_account* acct, std::shared_ptr<exchange> network)
{
  currency_ = *c;
  if (acct) account_ = acct;
  if (network) network_ = network;
  if (c->code_.size() == 40) { ui->currency->setText(hex_to_currency(c->code_).c_str()); }
  else { ui->currency->setText(c->code_.c_str()); }
  ui->issuer->setText(c->issuer_.c_str());
  //
  ui->balance->setText(to_string(c->balance_, *c).c_str());
  ui->avail->setText(to_string(c->avail_, *c).c_str());
  ui->reserved->setText(to_string(c->reserved_, *c).c_str());
  update();
  /*
    ui.bitstamp_xrp_fee->setText(boost::str(boost::format("fee %.4f%%") %     global_settings.bitstamp_xrp_fee).c_str());
*/
}

// ----------------------------------------------------------------------------
void currency_widget::transfer_setup_xrp(double fraction)
{
  amount_ = fraction * currency_.avail_;
  ui->amount_edit->setText(to_string(amount_, currency_).c_str());
}

// ----------------------------------------------------------------------------
void currency_widget::q1x_clicked() { transfer_setup_xrp(0.25); }
void currency_widget::q2x_clicked() { transfer_setup_xrp(0.50); }
void currency_widget::q3x_clicked() { transfer_setup_xrp(0.75); }
void currency_widget::q4x_clicked() { transfer_setup_xrp(1.00); }

// ----------------------------------------------------------------------------
void currency_widget::show_hide()
{
  if (ui->controls_pay->isHidden())
  {
    ui->dest_combo->clear();

    // for each walleet on each network
    for (auto network : global_settings.networks_)
    {
      for (auto w : network->wallets())
      {
        if (network_->can_send(currency_, w->network_.get()))
        {
          QVariant v;
          v.setValue(w);
          ui->dest_combo->addItem(QString(w->name_.c_str()), v);
        }
      }
    }

    QVariant v;
    v.setValue(nullptr);
    ui->dest_combo->addItem(QString("other XRP address"), v);

    /*
        // Add bitstamp exchange to transfer list
        if (network_->can_send(currency_,     global_settings.bitstamp.network_.get())) {
            QVariant v;
            v.setValue(static_cast<basic_account*>(&    global_settings.bitstamp));
            ui->dest_combo->addItem(QString(    global_settings.bitstamp.name_.c_str()), v);
        }

        // Add xrpl exchange wallets to transfer list
        for (auto & w:     global_settings.xrpl_wallets) {
            if (network_->can_send(currency_, w.network_.get())) {
                QVariant v;
                v.setValue(static_cast<basic_account*>(&w));
                ui->dest_combo->addItem(QString(w.name_.c_str()), v);
            }
        }
*/
    ui->buy_sell_combo->clear();
    auto pairs = network_->get_currency_pairs();
    for (auto& p : pairs)
    {
      auto c1 = std::get<0>(p);
      auto c2 = std::get<1>(p);
      if (c1 == currency_)
      {
        ui->buy_sell_combo->addItem(QString(c2.code_.c_str()), QString(c2.issuer_.c_str()));
      }
      else if (c2 == currency_)
      {
        ui->buy_sell_combo->addItem(QString(c1.code_.c_str()), QString(c1.issuer_.c_str()));
      }
    }
    //
    buy_sell_status();
    //
    ui->controls_amount->show();
    ui->controls_pay->show();
    ui->controls_trade->show();
  }
  else
  {
    ui->controls_amount->hide();
    ui->controls_pay->hide();
    ui->controls_trade->hide();
  }
}

// ----------------------------------------------------------------------------
double currency_widget::get_amount() { return std::stod(ui->amount_edit->text().toStdString()); }

// ----------------------------------------------------------------------------
void currency_widget::execute_payment()
{
  amount_ = get_amount();
  std::cout << "Transferring " << amount_ << " to " << ui->dest_combo->currentText().toStdString()
            << std::endl;
  // ????
  currency payment = this->currency_;
  payment.balance_ = amount_;
  QVariant v = ui->dest_combo->currentData();
  basic_account* to_wallet = v.value<basic_account*>();
  if (to_wallet != nullptr) { network_->make_payment(payment, account_, to_wallet); }
  else
  {
    ledger_wallet w;
    w.public_ = ui->address->text().toStdString();
    w.tag_ = (int64_t) (ui->tag->text().toInt());
    w.testnet_ = false;
    network_->make_payment(payment, account_, &w);
  }
}

void currency_widget::execute_trade()
{
  bool feesincluded = ui->FeesIncluded->isChecked();
  int N = ui->num_orders->value();
  currency_code taker_payc =
      currency_code{ui->buy_sell_combo->currentData().toString().toStdString(),
          ui->buy_sell_combo->currentText().toStdString()};
  //
  if (ui->amount_edit->text().isEmpty()) return;

  // total amount to be used
  double taker_gets = ui->amount_edit->text().toDouble();
  if (taker_gets == 0) return;

  QString now(QDateTime::currentDateTime().toString("dd.MM.yy hh:mm:ss"));
  double price_min = ui->min_price->value();
  double price_max = ui->max_price->value();

  // if taker pays us xrp, we are buying it
  bool buy_order = (taker_payc.is_xrp());

  // smaller per order amount for N orders
  std::vector<trade_data> trades;
  for (int i = 0; i < N; ++i)
  {
    double taker_get;
    double taker_pay;
    double price;
    if (N > 1)
      price = price_min + i * (price_max - price_min) / (N - 1);
    else
      price = price_min;
    if (buy_order)
    {
      taker_get = taker_gets / N;
      taker_pay = taker_gets / (price * N);
    }
    else
    {
      taker_get = taker_gets / N;
      taker_pay = taker_get * price;
    }
    //
    double fee_percent = network_->get_transaction_fee_percent({taker_payc, this->currency_});
    if (feesincluded)
    {
      taker_get = (1.0 - 0.01 * fee_percent) * taker_get;
      taker_pay = (1.0 - 0.01 * fee_percent) * taker_pay;
    }
    //
    trade_data t{
        network_,
        account_->name_,
        taker_payc,         // taker pays this currency
        this->currency_,    // taker gets this currency
        taker_pay,          // taker pays this amount (total)
        taker_get,          // taker gets this amount (total)
        price,              // exchange rate : TODO - check fee settings
        network_->get_transaction_fee_percent({taker_payc, this->currency_}),
        network_->get_transaction_fee_percent({taker_payc, this->currency_}),
        0,    // Id
        now.toStdString(),
        false,
    };
    trades.push_back(t);
  }
  check_trades_dialog d(this, trades);
  if (d.exec() == QDialog::Accepted) { network_->place_buy_sell_orders(account_, trades); }
}

// ----------------------------------------------------------------------------
void currency_widget::buy_sell_status()
{
  bool buy = currency_.is_fiat();
  if (buy)
  {
    QPalette palette = ui->buy_sell->palette();
    palette.setColor(QPalette::WindowText, QRgb(0x00'CF00));
    ui->buy_sell->setPalette(palette);
    ui->buy_sell->setText(
        "Buy " + ui->buy_sell_combo->currentText() + " <- " + currency_.code_.c_str());
  }
  else
  {
    QPalette palette = ui->buy_sell->palette();
    palette.setColor(QPalette::WindowText, QRgb(0xFF'4040));
    ui->buy_sell->setPalette(palette);
    ui->buy_sell->setText(
        QString("Sell ") + currency_.code_.c_str() + " -> " + ui->buy_sell_combo->currentText());
  }
}
