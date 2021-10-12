#include "trade_widget.hpp"
#include "ui_trade_widget.h"

#include "nlohmann/json.hpp"

trade_widget::trade_widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::trade_widget)
{
    ui->setupUi(this);
    connect_events();
}

trade_widget::trade_widget(std::string_view data, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::trade_widget)
{
    ui->setupUi(this);
    connect_events();
    //
    nlohmann::json jdata = nlohmann::json::parse(data);
    //
    // [{"price": "1.29000", "currency_pair": "XRP/USD", "datetime": "2021-05-30 20:55:34", "amount": "50000.00000000", "type": "1", "id": "1366315662319616"}]
    ui->network->setText("Bitstamp");
    ui->taker_getc->setText(jdata["currency_pair"].get<std::string>().c_str());
    ui->taker_payc->setText(jdata["currency_pair"].get<std::string>().c_str());
    ui->taker_get->setText(jdata["amount"].get<std::string>().c_str());
    ui->taker_pay->setText(jdata["price"].get<std::string>().c_str());
    ui->date_time->setText(jdata["datetime"].get<std::string>().c_str());
}

void trade_widget::set_data(trade_data const &t)
{
    trade_ = t;
    std::string temp = t.network_->name().begin();
    QPalette palette = ui->network->palette();
    palette.setColor(QPalette::WindowText, QRgb(0x2020FF));
    ui->network->setPalette(palette);
    ui->network->setText(QString::fromStdString(temp));
    //
    if (t.get_trade_type() == trade_type::buy) {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0x00FF00));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText("Buy");
        //
        ui->taker_getc->setText(to_string(t.taker_getc_).first.c_str());
        ui->taker_payc->setText(to_string(t.taker_payc_).first.c_str());
        //
        ui->taker_get->setText(to_string(t.taker_get_, t.taker_getc_).c_str());
        ui->taker_pay->setText(to_string(t.taker_pay_, t.taker_payc_).c_str());
    }
    else {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0xFF0000));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText("Sell");
        //
        ui->taker_getc->setText(to_string(t.taker_payc_).first.c_str());
        ui->taker_payc->setText(to_string(t.taker_getc_).first.c_str());
        //
        ui->taker_get->setText(to_string(t.taker_pay_, t.taker_payc_).c_str());
        ui->taker_pay->setText(to_string(t.taker_get_, t.taker_getc_).c_str());
    }
    //
    ui->exchange_rate->setText(std::to_string(t.exchange_rate_).c_str());
    // fee shown in currency of our trade
    ui->fee->setText(to_string_with_precision(t.fee_percent_, 2).c_str() + QString("%"));
    ui->id->setText(std::to_string(t.id_).c_str());
    ui->date_time->setText(t.datetime_.c_str());
}

trade_widget::~trade_widget()
{
    delete ui;
}

void trade_widget::connect_events()
{
    connect(ui->cancel, &QToolButton::clicked, this, [this]() {
        trade_.network_->cancel_order(trade_);
    });
}
