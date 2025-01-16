#pragma once

// STL
#include <string>
#include <vector>
// Qt
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QValidator>
// Grox
#include "config/config.hpp"
#include "data/ohlc_dataset.hpp"
#include "exchange/exchange.hpp"
#include "indicators/indicator_params.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
// we must provide one overload for each type in indicators::param_types
QWidget* get_widget(double const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QDoubleValidator(-10.0E9, 10.0E9, 6, widget));
  widget->setText(QString::number(param));
  return widget;
}

// ----------------------------------------------------------------------------
QWidget* get_widget(int const& param)
{
  QLineEdit* const widget = new QLineEdit();
  widget->setValidator(new QIntValidator(0, 65535, widget));
  widget->setText(QString::number(param));
  return widget;
}

// ----------------------------------------------------------------------------
QWidget* get_widget(ohlc_modes const& param)
{
  QStringList mode_list;
  for (auto const& r : ohlc_mode_names) { mode_list << QString::fromStdString(std::string(r)); }

  QComboBox* const widget = new QComboBox();
  widget->addItems(mode_list);
  widget->setCurrentText(QString::fromStdString(std::string(magic_enum::enum_name(param))));
  return widget;
}

// ----------------------------------------------------------------------------
QWidget* get_widget(bool const& param)
{
  QCheckBox* const widget = new QCheckBox();
  widget->setChecked(param);
  return widget;
}

// ----------------------------------------------------------------------------
QWidget* get_widget(candle_data const& param)
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
QWidget* get_widget(order_book_param const& param)
{
  QFrame* widget = new QFrame();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setLayout(layout);

  // put all the exchanges we know about into the combox box
  QComboBox* const exchange = new QComboBox(widget);
  exchange->setObjectName("Exchange");
  layout->addWidget(exchange);
  QStringList qsl;

  QComboBox* const ticker = new QComboBox(widget);
  ticker->setObjectName("Ticker");
  layout->addWidget(ticker);

  // this lambda will set the ticker combo using the tickers available from the exchange
  auto set_ticker_strings = [exchange, ticker, param](int index) {
    auto exchange = global_settings.networks_[index];
    currency_pair::list const& cplist = exchange->get_currency_pairs();
    auto tickers = exchange->tickers_subscribed();
    QStringList temp;
    for (auto const [cp, td] : tickers) { temp << currency_pair_qstring(cp); }
    ticker->clear();
    ticker->addItems(temp);
    ticker->setCurrentText(currency_pair_qstring(param.ticker_));
  };

  QWidget::connect(exchange, &QComboBox::currentIndexChanged, widget,
      [=](int index) { set_ticker_strings(index); });

  // trigger the exchange combo to update and fill the tickers combo
  for (auto const& n : global_settings.networks_) { qsl << to_qstring(n->get_name()); }
  exchange->addItems(qsl);
  exchange->setCurrentText(to_qstring(param.exchange_));
  //
  return widget;
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<double>& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param.put(QLocale().toDouble(w->text(), nullptr));
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<int>& param)
{
  QLineEdit* w = dynamic_cast<QLineEdit*>(widget);
  param.put(QLocale().toInt(w->text(), nullptr));
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<bool>& param)
{
  QCheckBox* w = dynamic_cast<QCheckBox*>(widget);
  param.put(w->isChecked());
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<candle_data>& param)
{
  QFrame* f = dynamic_cast<QFrame*>(widget);
  QComboBox* c = f->findChild<QComboBox*>("CandleRes");
  int index = c->currentIndex();
  candle_res res = ohlc_data_resolutions::available_resolutions()[index];

  QComboBox* d = f->findChild<QComboBox*>("TimeRange");
  std::string s = d->currentText().toLatin1().data();
  std::uint64_t samples = candle_data::samples(res, s);
  param.put({res, samples});
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<ohlc_modes>& param)
{
  QComboBox* w = dynamic_cast<QComboBox*>(widget);
  int index = w->currentIndex();
  param.put(magic_enum::enum_value<ohlc_modes>(index));
}

// ----------------------------------------------------------------------------
void set_param(QWidget* widget, indicators::param<order_book_param>& param)
{
  QFrame* f = dynamic_cast<QFrame*>(widget);
  QComboBox* e = f->findChild<QComboBox*>("Exchange");
  std::string exch = e->currentText().toLatin1().data();
  //
  QComboBox* t = f->findChild<QComboBox*>("Ticker");
  std::string s = t->currentText().toLatin1().data();
  currency_pair cp = string_to_pair(s, "-");
  //
  param.put({exch, cp});
}

// ----------------------------------------------------------------------------
ticker_data init_order_book_params(order_book_param const& p, QLabel* l1, QLabel* l2)
{
  std::string name = p.exchange_;
  currency_pair cp = p.ticker_;
  l1->setText(name.c_str());
  l2->setText(currency_pair_qstring(cp));
  //
  auto it = std::find_if(global_settings.networks_.begin(), global_settings.networks_.end(),
      [name](auto n) { return n->get_name() == name; });
  auto tdata = (*it)->get_subscribed_ticker_data(cp);
  //
  return tdata;
}
