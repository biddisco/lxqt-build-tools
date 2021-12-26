#include <QDialog>
#include <QMessageBox>
//
#include "xrp_functions.hpp"
#include "trade_widget.hpp"
#include "ui_xrp_functions.h"
#include "check_trades_dialog.hpp"
//
#include <string>
#include <boost/format.hpp>
//
#include "src/exchange/exchange.hpp"
#include "src/exchange/xrpl_network.hpp"

// ----------------------------------------------------------------------------
xrp_functions::xrp_functions(xrpl_network *network, basic_account *account, QWidget *parent)
    : QWidget(parent)
    , currency_{}
    , network_(network)
    , account_(account)
    , ui(new Ui::xrp_functions)
{
    ui->setupUi(this);
//    ui->amount_edit->setValidator( new QDoubleValidator(0, 1E9, 6, this) );
    //
//    connect(ui->q1x, SIGNAL(clicked()), this, SLOT(q1x_clicked()));
//    connect(ui->q2x, SIGNAL(clicked()), this, SLOT(q2x_clicked()));
//    connect(ui->q3x, SIGNAL(clicked()), this, SLOT(q3x_clicked()));
//    connect(ui->q4x, SIGNAL(clicked()), this, SLOT(q4x_clicked()));
//    connect(ui->show, SIGNAL(clicked()), this, SLOT(show_hide()));
//    connect(ui->exec_pay, SIGNAL(clicked()), this, SLOT(execute_payment()));
    connect(ui->exec_trustline, &QToolButton::clicked, this, [this](){
        std::string addr = ui->address->text().toStdString();
        std::string code = ui->code->text().toStdString();
        uint64_t limit = ui->limit->value();
        std::cout << addr << " " << code << " " << limit << std::endl;
        network_->trustline(account_, addr, code, limit);
    });
//    connect(ui->amount_edit, SIGNAL(returnPressed()), this, SLOT(get_amount()));

//    connect(ui->buy_sell_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
//        [=](int /*index*/){ buy_sell_status(); });
}

// ----------------------------------------------------------------------------
xrp_functions::~xrp_functions()
{
    delete ui;
}

// ----------------------------------------------------------------------------
void xrp_functions::exec_trustline()
{
}


// ----------------------------------------------------------------------------
//void xrp_functions::set_data(currency const *c, basic_account *acct, std::shared_ptr<exchange> network)
//{
//    currency_ = *c;
//    if (acct) account_ = acct;
//    if (network) network_ = network;
//    ui->currency->setText(c->name_.c_str());
//    ui->issuer->setText(c->issuer_.c_str());
//    //
//    ui->balance->setText(to_string(c->balance_, c->type_).c_str());
//    ui->avail->setText(to_string(c->avail_, c->type_).c_str());
//    ui->reserved->setText(to_string(c->reserved_, c->type_).c_str());
//    update();
///*
//    ui.bitstamp_xrp_fee->setText(boost::str(boost::format("fee %.4f%%") % app_ini->bitstamp_xrp_fee).c_str());
//*/
//}

// ----------------------------------------------------------------------------
//void xrp_functions::transfer_setup_xrp(double fraction) {
//    amount_ = fraction * currency_.avail_;
//    ui->amount_edit->setText(to_string(amount_, currency_.type_).c_str());
//}

// ----------------------------------------------------------------------------
//void xrp_functions::q1x_clicked() { transfer_setup_xrp(0.25); }
//void xrp_functions::q2x_clicked() { transfer_setup_xrp(0.50); }
//void xrp_functions::q3x_clicked() { transfer_setup_xrp(0.75); }
//void xrp_functions::q4x_clicked() { transfer_setup_xrp(1.00); }

// ----------------------------------------------------------------------------
//void xrp_functions::show_hide()
//{
//    if (ui->controls_pay->isHidden()) {
//        ui->dest_combo->clear();
//        app_settings* app_ini = global_settings();

//        // for each walleet on each network
//        for (auto network : app_ini->networks_) {
//            for (auto w : network->wallets()) {
//                if (network_->can_send(currency_, w->network_.get())) {
//                    QVariant v;
//                    v.setValue(w);
//                    ui->dest_combo->addItem(QString(w->name_.c_str()), v);
//                }
//            }
//        }
///*
//        // Add bitstamp exchange to transfer list
//        if (network_->can_send(currency_, app_ini->bitstamp.network_.get())) {
//            QVariant v;
//            v.setValue(static_cast<basic_account*>(&app_ini->bitstamp));
//            ui->dest_combo->addItem(QString(app_ini->bitstamp.name_.c_str()), v);
//        }

//        // Add xrpl exchange wallets to transfer list
//        for (auto & w: app_ini->xrpl_wallets) {
//            if (network_->can_send(currency_, w.network_.get())) {
//                QVariant v;
//                v.setValue(static_cast<basic_account*>(&w));
//                ui->dest_combo->addItem(QString(w.name_.c_str()), v);
//            }
//        }
//*/
//        ui->buy_sell_combo->clear();
//        auto pairs = network_->currency_pairs();
//        for (auto &p : pairs) {
//            auto c1 = p.first;
//            auto c2 = p.second;
//            if (c1 == currency_.type_) {
//                auto cstr = to_string(c2);
//                ui->buy_sell_combo->addItem(QString(cstr.first.c_str()),
//                                            QString(cstr.second.c_str()));
//            }
//            else {
//                // we don't show trade pairs the other way around (yet?)
//                //ui->buy_sell_combo->addItem(QString(to_string(c1).begin()));
//            }
//        }
//        //
//        buy_sell_status();
//        //
//        ui->controls_amount->show();
//        ui->controls_pay->show();
//        ui->controls_trade->show();
//    }
//    else {
//        ui->controls_amount->hide();
//        ui->controls_pay->hide();
//        ui->controls_trade->hide();
//    }
//}

//// ----------------------------------------------------------------------------
//double xrp_functions::get_amount()
//{
//    return std::stod(ui->amount_edit->text().toStdString());
//}

//// ----------------------------------------------------------------------------
//void xrp_functions::execute_payment()
//{
//    amount_ = get_amount();
//    std::cout << "Transferring " << amount_ << " to " << ui->dest_combo->currentText().toStdString() << std::endl;
//    // ????
//    currency payment = this->currency_;
//    payment.balance_ = amount_;
//    QVariant v = ui->dest_combo->currentData();
//    basic_account *to_wallet = v.value<basic_account*>();
//    network_->make_payment(payment, account_, to_wallet);
//}

//void xrp_functions::execute_trade()
//{
//    int N = ui->num_orders->value();
//    currency_type taker_payc = get_currency_type(
//                ui->buy_sell_combo->currentText().toStdString(),
//                ui->buy_sell_combo->currentData().toString().toStdString());
//    //
//    if (ui->amount_edit->text().isEmpty())
//        return;
//    //
//    double taker_gets = ui->amount_edit->text().toDouble();
//    if (taker_gets==0)
//        return;

//    QString now(QDateTime::currentDateTime().toString("dd.MM.yy hh:mm:ss"));
//    double price_min = ui->min_price->value();
//    double price_max = ui->max_price->value();

//    // if taker pays us xrp, we are buying it
//    bool buy_order = (taker_payc == currency_type::xrp);
//    //
//    std::vector<trade_data> trades;
//    for (int i=0; i<N; ++i) {
//        double taker_get;
//        double taker_pay;
//        double price;
//        if (N>1) price = price_min + i*(price_max-price_min)/(N-1);
//        else price = price_min;
//        if (buy_order) {
//            taker_get = taker_gets/N;
//            taker_pay = taker_gets/(price*N);
//        }
//        else {
//            taker_get = taker_gets/N;
//            taker_pay = taker_get*price;
//        }
//        trade_data t{
//                    network_,
//                    account_->name_,
//                    taker_payc,             // taker pays this currency
//                    this->currency_.type_,  // taker gets this currency
//                    taker_pay,              // taker pays this amount (total)
//                    taker_get,              // taker gets this amount (total)
//                    price,                  // exchange rate
//                    network_->get_fee_percent(taker_payc, this->currency_.type_),
//                    network_->get_fee_percent(taker_payc, this->currency_.type_),
//                    0,          // Id
//                    now.toStdString(),
//        };
//        trades.push_back(t);
//    }
//    check_trades_dialog d(this, trades);
//    if (d.exec()==QDialog::Accepted) {
//        network_->place_buy_sell_orders(account_, trades);
//    }
//}

//// ----------------------------------------------------------------------------
//void xrp_functions::buy_sell_status()
//{
//    bool buy = is_fiat(currency_.type_);
//    if (buy) {
//        QPalette palette = ui->buy_sell->palette();
//        palette.setColor(QPalette::WindowText, QRgb(0x00CF00));
//        ui->buy_sell->setPalette(palette);
//        ui->buy_sell->setText("Buy " + ui->buy_sell_combo->currentText()
//            + " <- " + currency_.name_.c_str());
//    }
//    else {
//        QPalette palette = ui->buy_sell->palette();
//        palette.setColor(QPalette::WindowText, QRgb(0xFF4040));
//        ui->buy_sell->setPalette(palette);
//        ui->buy_sell->setText(QString("Sell ") + currency_.name_.c_str()
//            + " -> " + ui->buy_sell_combo->currentText());

//    }
//}
