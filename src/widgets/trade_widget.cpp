#include <utility>
//
#include "debug/print.hpp"
#include "trade_widget.hpp"
#include "ui_trade_widget.h"
#include "util/stringutils.hpp"

#include "exchange/bitstamp.hpp"
#include "nlohmann/json.hpp"
#include "senders/qtstdexec.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
template <int Level>
inline constexpr print_threshold<Level, 3> trade_dbg("TradeWgt");

// ----------------------------------------------------------------------------
trade_widget::trade_widget(QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::trade_widget)
{
  ui->setupUi(this);
  connect_events();
}

// ----------------------------------------------------------------------------
trade_widget::trade_widget(std::string_view data, QWidget* parent)
  : QWidget(parent)
  , ui(new Ui::trade_widget)
{
  ui->setupUi(this);
  connect_events();
  //
  using namespace nlohmann;
  nlohmann::json jdata = json::parse(data);
  //
  // [{"price": "1.29000", "currency_pair": "XRP/USD", "datetime": "2021-05-30 20:55:34", "amount": "50000.00000000", "type": "1", "id": "1366315662319616"}]
  ui->network->setText("Bitstamp");
  ui->taker_getc->setText(jdata["currency_pair"].get_ptr<json::string_t*>()->c_str());
  ui->taker_payc->setText(jdata["currency_pair"].get_ptr<json::string_t*>()->c_str());
  ui->taker_get->setText(jdata["amount"].get_ptr<json::string_t*>()->c_str());
  ui->taker_pay->setText(jdata["price"].get_ptr<json::string_t*>()->c_str());
  ui->date_time->setText(jdata["datetime"].get_ptr<json::string_t*>()->c_str());
}

// ----------------------------------------------------------------------------
void trade_widget::set_data(trade_data const& t)
{
  trade_ = t;
  QPalette palette = ui->network->palette();
  palette.setColor(QPalette::WindowText, QRgb(0x20'20FF));
  ui->network->setPalette(palette);
  ui->network->setText(QString::fromStdString(t.network_->get_name()));
  //
  if (t.get_trade_type() == trade_type::buy)
  {
    QPalette palette = ui->buy_sell->palette();
    palette.setColor(QPalette::WindowText, QRgb(0x00'FF00));
    ui->buy_sell->setPalette(palette);
    ui->buy_sell->setText("Buy");
    //
    ui->taker_getc->setText(t.taker_getc_.code_.c_str());
    ui->taker_payc->setText(t.taker_payc_.code_.c_str());
    //
    ui->taker_get->setText(currency_precision(t.taker_get_, t.taker_getc_).c_str());
    ui->taker_pay->setText(currency_precision(t.taker_pay_, t.taker_payc_).c_str());
  }
  else
  {
    QPalette palette = ui->buy_sell->palette();
    palette.setColor(QPalette::WindowText, QRgb(0xFF'0000));
    ui->buy_sell->setPalette(palette);
    ui->buy_sell->setText("Sell");
    //
    ui->taker_getc->setText(t.taker_payc_.code_.c_str());
    ui->taker_payc->setText(t.taker_getc_.code_.c_str());
    //
    ui->taker_get->setText(currency_precision(t.taker_pay_, t.taker_payc_).c_str());
    ui->taker_pay->setText(currency_precision(t.taker_get_, t.taker_getc_).c_str());
  }
  //
  ui->exchange_rate->setText(std::to_string(t.exchange_rate_).c_str());
  // fee shown in currency of our trade
  ui->fee->setText(to_string_with_precision(t.fee_percent_, 2).c_str() + QString("%"));

  // Id label colour red/green for unconfirmed/confirmed
  QColor color = t.confirmed_ ? QColor(Qt::darkGreen) : QColor(Qt::red);
  QPalette lpalette = ui->id->palette();
  lpalette.setColor(QPalette::WindowText, color);
  ui->id->setPalette(lpalette);
  ui->id->setText(std::to_string(t.id_).c_str());
  ui->date_time->setText(t.datetime_.c_str());
}

// ----------------------------------------------------------------------------
trade_widget::~trade_widget() { delete ui; }

// ----------------------------------------------------------------------------
void trade_widget::connect_events()
{
  connect(ui->cancel, &QToolButton::clicked, this, [this]() {
    auto web = stdexec::starts_on(QtStdExec::QThreadScheduler(), stdexec::just())    // Qt
        | stdexec::let_value(                                                        //
              [this]() { return trade_.network_->request_cancel_order(trade_); })    // Qt -> pika
        | stdexec::then([this](QByteArray byteArray) {                               // pika
            std::string_view data(byteArray.constData(), byteArray.length());
            nlohmann::json jdata = nlohmann::json::parse(data);
            trade_dbg<2>.debug(ffmt<s20>("cancel_order"), trade_.id_, jdata.dump());
            if (!jdata.contains("error"))
            {
              if (jdata["id"] == trade_.id_)
              {
                auto acct = std::dynamic_pointer_cast<bitstamp_network>(trade_.network_)->account();
                acct.remove_trade(trade_);
              }
              else { trade_dbg<0>.error(ffmt<s20>("cancel_order"), trade_.id_, jdata.dump()); }
            }
          });
    stdexec::start_detached(std::move(web));
  });
}
