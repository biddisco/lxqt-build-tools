#include "currency_widget.hpp"
#include "ui_currency_widget.h"
//
#include <string>
#include <boost/format.hpp>
//
#include "exchange/exchange.hpp"

// ----------------------------------------------------------------------------
currency_widget::currency_widget(int decimals, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::currency_widget),
    decimals_(decimals), currency_{}
{
    ui->setupUi(this);
    ui->controls->hide();
    ui->amount_edit->setValidator( new QDoubleValidator(0, 1E9, 6, this) );
    fmt_ = "%02." + std::to_string(decimals_) + "f";
    //
    connect(ui->q1x, SIGNAL(clicked()), this, SLOT(q1x_clicked()));
    connect(ui->q2x, SIGNAL(clicked()), this, SLOT(q2x_clicked()));
    connect(ui->q3x, SIGNAL(clicked()), this, SLOT(q3x_clicked()));
    connect(ui->q4x, SIGNAL(clicked()), this, SLOT(q4x_clicked()));
    connect(ui->show, SIGNAL(clicked()), this, SLOT(show_hide()));
    connect(ui->exec, SIGNAL(clicked()), this, SLOT(execute_transfer()));
    connect(ui->amount_edit, SIGNAL(returnPressed()), this, SLOT(get_amount()));
}

// ----------------------------------------------------------------------------
currency_widget::~currency_widget()
{
    delete ui;
}

// ----------------------------------------------------------------------------
void currency_widget::set_data(currency const *c, exchange *network)
{
    currency_ = *c;
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
    if (ui->controls->isHidden()) {
        ui->dest_combo->clear();
        app_settings* app_ini = global_settings();
        for (auto & w: app_ini->xrpl_wallets) {
            if (network_->can_send(currency_, w.network_.get()))
                ui->dest_combo->addItem(QString(w.name_.c_str()));
        }
        ui->controls->show();
    }
    else {
        ui->controls->hide();
    }
}

// ----------------------------------------------------------------------------
void currency_widget::get_amount()
{
    amount_ = std::stod(ui->amount_edit->text().toStdString());
}

// ----------------------------------------------------------------------------
void currency_widget::execute_transfer()
{
    amount_ = std::stod(ui->amount_edit->text().toStdString());
    std::cout << "Transferring " << amount_ << " to " << ui->dest_combo->currentText().toStdString() << std::endl;
    app_settings* app_ini = global_settings();
    for (auto & w: app_ini->xrpl_wallets) {
//        ui->dest_combo->addItem(QString(w.name_.c_str()));
    }
}
