#pragma once

#include <cstdint>
#include <string>
#include <vector>
//
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QValidator>
//
#include <range/v3/view.hpp>
#include <fmt/format.h>
//
#include "config/config.hpp"
#include "data/ohlc_dataset.hpp"
#include "exchange/abstract_exchange.hpp"
#include "indicators/indicator_params.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
// we must provide one overload for each type in indicators::param_types
static QWidget* get_widget(double const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QDoubleValidator(-10.0E9, 10.0E9, 6, widget));
  widget->setText(QString::number(param));
  return widget;
}

// ----------------------------------------------------------------------------
static QWidget* get_widget(int const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QIntValidator(0, 65535, widget));
  widget->setText(QString::number(param));
  return widget;
}

// ----------------------------------------------------------------------------
static QWidget* get_widget(ohlc_modes const& param)
{
  QStringList mode_list;
  for (auto const& r : ohlc_mode_names) { mode_list << QString::fromStdString(std::string(r)); }

  QComboBox* const widget = new QComboBox();
  widget->addItems(mode_list);
  widget->setCurrentText(QString::fromStdString(std::string(magic_enum::enum_name(param))));
  return widget;
}

// ----------------------------------------------------------------------------
static QWidget* get_widget(bool const& param)
{
  QCheckBox* const widget = new QCheckBox();
  widget->setChecked(param);
  return widget;
}

// ----------------------------------------------------------------------------
static QWidget* get_widget(candle_data const& param)
{
  QFrame* widget = new QFrame();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setLayout(layout);

  // combo box of resolutions to choose from
  QStringList res_list;
  for (auto const& r : ohlc_data_resolutions::available_resolutions()) { res_list << r.name_; }
  QComboBox* const combo = new QComboBox(widget);
  combo->setObjectName("CandleRes");
  combo->addItems(res_list);
  combo->setCurrentText(param.res_.name_);
  layout->addWidget(combo);

  // combo box of time ranges to choose from
  QStringList temp;
  for (auto const& s : candle_data::durations) { temp.push_back(s); }
  //
  QComboBox* const ticker = new QComboBox(widget);
  ticker->setObjectName("TimeRange");
  ticker->addItems(temp);
  ticker->setCurrentText(QString(param.as_string().c_str()));
  layout->addWidget(ticker);
  return widget;
}

// ----------------------------------------------------------------------------
static QWidget* get_widget(order_book_param const& param)
{
  QFrame* widget = new QFrame();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setObjectName("OrderBookWidget");
  widget->setLayout(layout);
  widget->setProperty("TSize", QVariant(static_cast<int>(param.tickers_.size())));

  // put all the exchanges we know about into the combox box
  QComboBox* const abstract_exchange = new QComboBox(widget);
  abstract_exchange->setObjectName("Exchange");
  layout->addWidget(abstract_exchange);

  std::vector<QComboBox*> qtickers;
  for (auto [i, t] : param.tickers_ | ranges::views::enumerate)
  {
    QComboBox* ticker = new QComboBox(widget);
    ticker->setObjectName(fmt::format("Ticker {}", i));
    layout->addWidget(ticker);
    qtickers.push_back(ticker);
  }

  // this lambda will set the ticker combo using the tickers available from the abstract_exchange
  auto set_ticker_strings = [abstract_exchange, &qtickers, param](int index) {
    auto abstract_exchange = global_settings.networks_[index];
    auto tickers = abstract_exchange->tickers_subscribed();
    for (auto [i, ticker] : qtickers | ranges::views::enumerate)
    {
      QStringList temp;
      for (auto const [cp, td] : tickers) { temp << currency_pair_qstring(cp); }
      ticker->clear();
      ticker->addItems(temp);
      ticker->setCurrentText(currency_pair_qstring(param.tickers_[i]));
    }
  };

  QWidget::connect(abstract_exchange, &QComboBox::currentIndexChanged, widget,
      [=](int index) { set_ticker_strings(index); });

  // trigger the abstract_exchange combo to update and fill the tickers combo
  QStringList qsl;
  for (auto const& n : global_settings.networks_) { qsl << to_qstring(n->get_name()); }
  abstract_exchange->addItems(qsl);
  abstract_exchange->setCurrentText(to_qstring(param.exchange_));
  //
  return widget;
}
// ----------------------------------------------------------------------------
static void orderbook_widget_constrain_networks(
    QWidget* widget, std::vector<std::shared_ptr<abstract_exchange>> networks)
{
  QComboBox* const abstract_exchange = widget->findChild<QComboBox*>("Exchange");
  QStringList qsl;
  for (auto const& n : networks) { qsl << to_qstring(n->get_name()); }
  // abstract_exchange->setUpdatesEnabled(false);
  // abstract_exchange->clear();
  // abstract_exchange->addItems(qsl);
  // abstract_exchange->setUpdatesEnabled(true);
  // abstract_exchange->setCurrentText(to_qstring(networks[0]->get_name()));
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<double>& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param.put(QLocale().toDouble(w->text(), nullptr));
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<int>& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param.put(QLocale().toInt(w->text(), nullptr));
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<bool>& param)
{
  QCheckBox* w = dynamic_cast<QCheckBox*>(widget);
  param.put(w->isChecked());
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<candle_data>& param)
{
  QFrame* f = dynamic_cast<QFrame*>(widget);
  QComboBox* c = f->findChild<QComboBox*>("CandleRes");
  int index = c->currentIndex();
  candle_res res = ohlc_data_resolutions::available_resolutions()[index];

  QComboBox* d = f->findChild<QComboBox*>("TimeRange");
  std::string s = d->currentText().toStdString();
  std::uint64_t samples = candle_data::samples(res, s);
  param.put({res, samples});
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<ohlc_modes>& param)
{
  QComboBox* w = dynamic_cast<QComboBox*>(widget);
  int index = w->currentIndex();
  param.put(magic_enum::enum_value<ohlc_modes>(index));
}

// ----------------------------------------------------------------------------
static void set_param(QWidget* widget, indicators::param<order_book_param>& param)
{
  QFrame* f = dynamic_cast<QFrame*>(widget);
  QComboBox* e = f->findChild<QComboBox*>("Exchange");
  std::string exch = e->currentText().toStdString();
  int num_tickers = f->property("TSize").value<int>();
  //
  currency_pair::list tickers;
  for (int i = 0; i < num_tickers; ++i)
  {
    QComboBox* t = f->findChild<QComboBox*>(fmt::format("Ticker {}", i));
    std::string s = t->currentText().toStdString();
    currency_pair cp = string_to_pair(s, "-");
    tickers.push_back(cp);
  }
  //
  param.put({exch, tickers});
}
