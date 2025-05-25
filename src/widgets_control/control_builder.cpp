#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <fmt/format.h>
//
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
//
#include "config/config.hpp"
#include "exchange/abstract_exchange.hpp"
#include "widgets_control/control_builder.hpp"

// ----------------------------------------------------------------------------
QWidget* control_builder_int::build(QWidget* parent, control_config const& config)
{
  auto* spin = new QSpinBox(parent);
  if (config.contains("min")) spin->setMinimum(config.at("min").toInt());
  if (config.contains("max")) spin->setMaximum(config.at("max").toInt());
  return spin;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_bool::build(QWidget* parent, control_config const& config)
{
  auto* box = new QCheckBox();
  if (config.contains("checked")) box->setChecked(config.at("checked").toBool());
  return box;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_double::build(QWidget* parent, control_config const& config)
{
  auto* edit = new QLineEdit();
  double value = 0;
  double min = -10.0E9;
  double max = 10.0E9;
  int decimals = 6;
  if (config.contains("min")) min = config.at("min").toDouble();
  if (config.contains("max")) max = config.at("max").toDouble();
  if (config.contains("decimals")) decimals = config.at("decimals").toInt();
  if (config.contains("value")) value = config.at("value").toInt();
  edit->setValidator(new QDoubleValidator(min, max, decimals, edit));
  edit->setText(QString::number(value));
  return edit;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_string::build(QWidget* parent, control_config const& config)
{
  auto* edit = new QLineEdit(parent);
  if (config.contains("placeholder")) edit->setPlaceholderText(config.at("placeholder").toString());
  return edit;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_combo::build(QWidget* parent, control_config const& config)
{
  auto* combo = new QComboBox(parent);
  if (config.contains("entries"))
  {
    combo->addItems(config.at("entries").toStringList());
    if (config.contains("index")) { combo->setCurrentIndex(config.at("index").toInt()); }
    else { combo->setCurrentText(config.at("entries").toStringList()[0]); }
  }
  else if (config.contains("placeholder"))
  {
    combo->setPlaceholderText(config.at("placeholder").toString());
  }
  return combo;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_ohlc_mode::build(QWidget* parent, control_config const& config)
{
  // just use a combo box for now
  auto builder = control_builder_combo();
  return builder.build(parent, config);
}

// ----------------------------------------------------------------------------
QWidget* control_builder_orderbook::build(QWidget* parent, control_config const& config)
{
  QFrame* widget = new QFrame();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setObjectName("OrderBookWidget");
  widget->setLayout(layout);

  // put exchange names into the combox box
  QComboBox* const exchanges = new QComboBox(widget);
  exchanges->setObjectName("Exchange");
  layout->addWidget(exchanges);
  if (config.contains("exchanges"))
  {
    auto list = config.at("exchanges").toStringList();
    exchanges->addItems(list);
    if (config.contains("exchange_index"))
    {
      exchanges->setCurrentIndex(config.at("exchange_index").toInt());
    }
    // if the choice is only limited to 1 item, no need to show it
    if (list.size() == 1) { exchanges->setVisible(false); }
  }

  std::vector<QComboBox*> qtickers;
  QComboBox* ticker1 = new QComboBox(widget);
  ticker1->setObjectName(fmt::format("Ticker_1"));
  layout->addWidget(ticker1);
  qtickers.push_back(ticker1);
  if (config.contains("tickers1"))
  {
    auto tickers = config.at("tickers1").toStringList();
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker1->addItems(tickers);
    if (config.contains("tickers1_index"))
    {
      ticker1->setCurrentIndex(config.at("tickers1_index").toInt());
    }
  }

  QComboBox* ticker2 = new QComboBox(widget);
  ticker2->setObjectName(fmt::format("Ticker_2"));
  layout->addWidget(ticker2);
  qtickers.push_back(ticker1);
  if (config.contains("tickers2"))
  {
    auto tickers = config.at("tickers2").toStringList();
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker2->addItems(tickers);
    if (config.contains("tickers2_index"))
    {
      ticker2->setCurrentIndex(config.at("tickers2_index").toInt());
    }
  }

  // this lambda will set the ticker combo using the tickers available from the exchanges
  auto set_ticker_strings = [&](int /*index*/) {
    std::string text = exchanges->currentText().toStdString();
    auto ex =
        ranges::find_if(global_settings.networks_, [&](auto e) { return (e->get_name() == text); });
    if (ex == global_settings.networks_.end()) return;
    //
    auto tickers = (*ex)->tickers_subscribed();
    for (auto [i, combo] : qtickers | ranges::views::enumerate)
    {
      QStringList temp;
      for (auto const [cp, td] : tickers) { temp << currency_pair_qstring(cp); }
      combo->clear();
      combo->addItems(temp);
      // combo->setCurrentText(currency_pair_qstring(param.tickers_[i]));
    }
  };

  QWidget::connect(exchanges, &QComboBox::currentIndexChanged, widget,
      [=](int index) { set_ticker_strings(index); });

  set_ticker_strings(0);

  // trigger the exchanges combo to update and fill the tickers combo
  // QStringList qsl;
  // for (auto const& n : global_settings.networks_) { qsl << to_qstring(n->get_name()); }
  // exchanges->addItems(qsl);
  // exchanges->setCurrentText(to_qstring(param.exchange_));
  //
  return widget;
}
