#include "currency_widget.hpp"
#include "ui_currency_widget.h"
//
#include <string>
#include <boost/format.hpp>
//
// ----------------------------------------------------------------------------
currency_widget::currency_widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::currency_widget), currency_{}
{
    ui->setupUi(this);
    ui->controls->hide();
    //
    connect(ui->q1x, SIGNAL(clicked()), this, SLOT(q1x_clicked()));
    connect(ui->q2x, SIGNAL(clicked()), this, SLOT(q2x_clicked()));
    connect(ui->q3x, SIGNAL(clicked()), this, SLOT(q3x_clicked()));
    connect(ui->q4x, SIGNAL(clicked()), this, SLOT(q4x_clicked()));
}

// ----------------------------------------------------------------------------
currency_widget::~currency_widget()
{
    delete ui;
}

// ----------------------------------------------------------------------------
void currency_widget::set_data(currency &c)
{
    currency_ = c;
    ui->currency->setText(c.name_.c_str());
    ui->issuer->setText(c.issuer_.c_str());
    ui->balance->setText(std::to_string(c.balance_).c_str());
    ui->avail->setText(std::to_string(c.avail_).c_str());
    ui->reserved->setText(std::to_string(c.reserved_).c_str());
}

// ----------------------------------------------------------------------------
void currency_widget::transfer_setup_xrp(double fraction) {
    double amt = fraction * currency_.avail_;
    ui->amount->setText(boost::str(boost::format("%.6f") % amt).c_str());
}

// ----------------------------------------------------------------------------
void currency_widget::q1x_clicked() { transfer_setup_xrp(0.25); }
void currency_widget::q2x_clicked() { transfer_setup_xrp(0.50); }
void currency_widget::q3x_clicked() { transfer_setup_xrp(0.75); }
void currency_widget::q4x_clicked() { transfer_setup_xrp(1.00); }
