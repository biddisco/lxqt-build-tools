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
    ui->currency_get->setText(jdata["currency_pair"].get<std::string>().c_str());
    ui->currency_pay->setText(jdata["currency_pair"].get<std::string>().c_str());
    ui->amount_get->setText(jdata["amount"].get<std::string>().c_str());
    ui->amount_pay->setText(jdata["price"].get<std::string>().c_str());
    ui->date_time->setText(jdata["datetime"].get<std::string>().c_str());
}

void trade_widget::set_data(trade_data const &t)
{
    trade_ = t;
    std::string temp = t.network_->name().begin();
    ui->network->setText(QString::fromStdString(temp));
    bool buy = t.trade_type_==0;
    if (buy) {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0x00FF00));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText("Buy");
    }
    else {
        QPalette palette = ui->buy_sell->palette();
        palette.setColor(QPalette::WindowText, QRgb(0xFF0000));
        ui->buy_sell->setPalette(palette);
        ui->buy_sell->setText("Sell");
    }
    //
    ui->currency_get->setText(to_string(t.currency_get_).begin());
    ui->currency_pay->setText(to_string(t.currency_pay_).begin());
    ui->amount_get->setText(std::to_string(t.amount_get_).c_str());
    ui->amount_pay->setText(std::to_string(t.amount_pay_).c_str());
    ui->value->setText(std::to_string(t.value_).c_str());
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
