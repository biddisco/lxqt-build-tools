#include "currency_widget.hpp"
#include "ui_currency_widget.h"
//
#include <string>
#include <boost/format.hpp>
//
#include "src/exchange/exchange.hpp"

// ----------------------------------------------------------------------------
currency_widget::currency_widget(int decimals, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::currency_widget),
    decimals_(decimals), currency_{}
{
    ui->setupUi(this);
    ui->controls_amount->hide();
    ui->controls_pay->hide();
    ui->controls_trade->hide();
    ui->amount_edit->setValidator( new QDoubleValidator(0, 1E9, 6, this) );
    fmt_ = "%02." + std::to_string(decimals_) + "f";
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
        [=](int /*index*/){ buy_sell_status(); });}

// ----------------------------------------------------------------------------
currency_widget::~currency_widget()
{
    delete ui;
}

// ----------------------------------------------------------------------------
void currency_widget::set_data(currency const *c, basic_account *acct, exchange *network)
{
    currency_ = *c;
    if (acct) account_ = acct;
    if (network) network_ = network;
    ui->currency->setText(c->name_.c_str());
    ui->issuer->setText(c->issuer_.c_str());
    //
    ui->balance->setText(boost::str(boost::format(fmt_) % c->balance_).c_str());
    ui->avail->setText(boost::str(boost::format(fmt_) % c->avail_).c_str());
    ui->reserved->setText(boost::str(boost::format(fmt_) % c->reserved_).c_str());
    update();
/*
    ui.bitstamp_xrp_fee->setText(boost::str(boost::format("fee %.4f%%") % app_ini->bitstamp_xrp_fee).c_str());
*/
}

// ----------------------------------------------------------------------------
void currency_widget::transfer_setup_xrp(double fraction) {
    amount_ = fraction * currency_.avail_;
    ui->amount_edit->setText(boost::str(boost::format(fmt_) % amount_).c_str());
}

// ----------------------------------------------------------------------------
void currency_widget::q1x_clicked() { transfer_setup_xrp(0.25); }
void currency_widget::q2x_clicked() { transfer_setup_xrp(0.50); }
void currency_widget::q3x_clicked() { transfer_setup_xrp(0.75); }
void currency_widget::q4x_clicked() { transfer_setup_xrp(1.00); }

// ----------------------------------------------------------------------------
void currency_widget::show_hide()
{
    if (ui->controls_pay->isHidden()) {
        ui->dest_combo->clear();
        app_settings* app_ini = global_settings();

        // Add bitstamp exchange to transfer list
        if (network_->can_send(currency_, app_ini->bitstamp.network_.get())) {
            QVariant v;
            v.setValue(static_cast<basic_account*>(&app_ini->bitstamp));
            ui->dest_combo->addItem(QString(app_ini->bitstamp.name_.c_str()), v);
        }

        // Add xrpl exchange wallets to transfer list
        for (auto & w: app_ini->xrpl_wallets) {
            if (network_->can_send(currency_, w.network_.get())) {
                QVariant v;
                v.setValue(static_cast<basic_account*>(&w));
                ui->dest_combo->addItem(QString(w.name_.c_str()), v);
            }
        }

        ui->buy_sell_combo->clear();
        auto pairs = network_->currency_pairs();
        for (auto &p : pairs) {
            auto c1 = p.first;
            auto c2 = p.second;
            if (c1 == currency_.type_) {
                ui->buy_sell_combo->addItem(QString(to_string(c2).begin()));
            }
            else {
                //ui->buy_sell_combo->addItem(QString(to_string(c1).begin()));
            }
        }
        //
        buy_sell_status();
        //
        ui->controls_amount->show();
        ui->controls_pay->show();
        ui->controls_trade->show();
    }
    else {
        ui->controls_amount->hide();
        ui->controls_pay->hide();
        ui->controls_trade->hide();
    }
}

// ----------------------------------------------------------------------------
double currency_widget::get_amount()
{
    return std::stod(ui->amount_edit->text().toStdString());
}

// ----------------------------------------------------------------------------
void currency_widget::execute_payment()
{
    amount_ = get_amount();
    std::cout << "Transferring " << amount_ << " to " << ui->dest_combo->currentText().toStdString() << std::endl;
    // ????
    currency payment = this->currency_;
    payment.balance_ = amount_;
    QVariant v = ui->dest_combo->currentData();
    basic_account *to_wallet = v.value<basic_account*>();
    network_->make_payment(payment, account_, to_wallet);
}

// ----------------------------------------------------------------------------
void currency_widget::execute_trade()
{

}

// ----------------------------------------------------------------------------
void currency_widget::buy_sell_status()
{
    bool buy = is_fiat(currency_.type_);
    if (buy) {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0x00CF00));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText("Buy " + ui->buy_sell_combo->currentText()
            + " <- " + currency_.name_.c_str());
    }
    else {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0xFF4040));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText(QString("Sell ") + currency_.name_.c_str()
            + " -> " + ui->buy_sell_combo->currentText());

    }
}
