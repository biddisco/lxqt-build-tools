#include "wallet_widget.hpp"
#include "ui_wallet_widget.h"
#include "currency_widget.hpp"
//
#include <boost/format.hpp>
//
#include "settings.hpp"
//
wallet_widget::wallet_widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wallet_widget)
{
    ui->setupUi(this);
}

wallet_widget::~wallet_widget()
{
    delete ui;
}

// ----------------------------------------------------------------------------
void wallet_widget::set_data(ledger_wallet &w)
{
    ui->ledger_wallet->setTitle(w.name_.c_str());
    ui->address->setText(w.public_.c_str());
    ui->tag->setText(QString(":") + QString(std::to_string(w.tag_).c_str()));
    //
    for (auto &c : w.currencies_) {
        if (c.widget_ == nullptr) {
            c.widget_ = new currency_widget(6, this);
            c.widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            ui->currencies_layout->addWidget(c.widget_);
        }
        c.widget_->set_data(&c, &w, w.network_);
    }
    update();
}

// ----------------------------------------------------------------------------
void wallet_widget::set_data(bitstamp_account &w)
{
    ui->ledger_wallet->setTitle("Bitstamp");
    ui->address->setText(w.public_.c_str());
    ui->tag->setText(QString(std::to_string(w.tag_).c_str()));
    //
    for (auto &c : w.currencies_) {
        if (c.widget_ == nullptr) {
            c.widget_ = new currency_widget(2, this);
            c.widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            ui->currencies_layout->addWidget(c.widget_);
        }
        c.widget_->set_data(&c, &w, w.network_);
    }
    update();
}
