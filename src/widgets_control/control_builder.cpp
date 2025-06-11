#include <list>
#include <map>
#include <memory>
#include <string>
//
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
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
#include "indicators/indicator_types.hpp"
#include "util/stringutils.hpp"
#include "widgets_control/control_builder.hpp"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
QWidget* control_builder_int::build(QWidget* parent, nlohmann::json const& defaults)
{
  auto* spin = new QSpinBox(parent);
  if (defaults.contains("min")) spin->setMinimum(defaults["min"].get<int>());
  if (defaults.contains("max")) spin->setMaximum(defaults["max"].get<int>());
  if (defaults.contains("value")) spin->setValue(defaults["value"].get<int>());
  return spin;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_bool::build(QWidget* parent, nlohmann::json const& defaults)
{
  auto* box = new QCheckBox();
  if (defaults.contains("value")) box->setChecked(defaults["value"].get<bool>());
  return box;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_double::build(QWidget* parent, nlohmann::json const& defaults)
{
  auto* edit = new QLineEdit(parent);
  double value = 0;
  double min = -10.0E9;
  double max = 10.0E9;
  int decimals = 6;
  if (defaults.contains("min")) min = defaults["min"].get<double>();
  if (defaults.contains("max")) max = defaults["max"].get<double>();
  if (defaults.contains("decimals")) decimals = defaults["decimals"].get<int>();
  if (defaults.contains("value")) value = defaults["value"].get<double>();
  edit->setValidator(new QDoubleValidator(min, max, decimals, edit));
  edit->setText(QString::number(value));
  return edit;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_string::build(QWidget* parent, nlohmann::json const& defaults)
{
  auto* edit = new QLineEdit(parent);
  if (defaults.contains("placeholder"))
    edit->setPlaceholderText(to_qstring(defaults["placeholder"].get<std::string>()));
  if (defaults.contains("value")) edit->setText(to_qstring(defaults["value"].get<std::string>()));
  return edit;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_combo::build(QWidget* parent, nlohmann::json const& defaults)
{
  auto* combo = new QComboBox(parent);
  if (defaults.contains("entries"))
  {
    combo->addItems(to_qstringlist(defaults["entries"].get<std::vector<std::string>>()));
    if (defaults.contains("index")) { combo->setCurrentIndex(defaults["index"].get<int>()); }
    else
    {
      combo->setCurrentText(to_qstringlist(defaults["entries"].get<std::vector<std::string>>())[0]);
    }
  }
  else if (defaults.contains("placeholder"))
  {
    combo->setPlaceholderText(to_qstring(defaults["placeholder"].get<std::string>()));
  }
  return combo;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_ohlc_mode::build(QWidget* parent, nlohmann::json const& defaults)
{
  // just use a combo box for now
  auto builder = control_builder_combo();
  return builder.build(parent, defaults);
}

// ----------------------------------------------------------------------------
QWidget* control_builder_candle_data::build(QWidget* parent, nlohmann::json const& defaults)
{
  QFrame* widget = new QFrame(parent);
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setLayout(layout);

  // combo box of resolutions to choose from
  QComboBox* const combo = new QComboBox(widget);
  QStringList resolutions;
  if (defaults.contains("resolutions"))
    resolutions = to_qstringlist(defaults["resolutions"].get<std::vector<std::string>>());
  else
    for (auto const& r : ohlc_data_resolutions::available_resolutions()) { resolutions << r.name_; }
  combo->setObjectName("resolution");
  combo->addItems(resolutions);
  if (defaults.contains("resolution"))
    combo->setCurrentText(to_qstring(defaults["resolution"].get<std::string>()));
  layout->addWidget(combo);

  // combo box of time ranges to choose from
  QComboBox* const ticker = new QComboBox(widget);
  QStringList durations;
  if (defaults.contains("durations"))
    durations = to_qstringlist(defaults["durations"].get<std::vector<std::string>>());
  else
    for (auto const& s : candle_data::durations) { durations.push_back(s); }
  ticker->setObjectName("durations");
  ticker->addItems(durations);
  if (defaults.contains("duration"))
    ticker->setCurrentText(to_qstring(defaults["duration"].get<std::string>()));
  layout->addWidget(ticker);
  return widget;
}

// ----------------------------------------------------------------------------
QWidget* control_builder_orderbook::build(QWidget* parent, nlohmann::json const& defaults)
{
  QFrame* widget = new QFrame();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  widget->setObjectName("OrderBookWidget");
  widget->setLayout(layout);

  // put exchange names into the combox box
  QComboBox* const exchanges = new QComboBox(widget);
  exchanges->setObjectName("Exchange");
  layout->addWidget(exchanges);
  if (defaults.contains("exchanges"))
  {
    auto list = to_qstringlist(defaults["exchanges"].get<std::vector<std::string>>());
    exchanges->addItems(list);
    if (defaults.contains("exchange_index"))
    {
      exchanges->setCurrentIndex(defaults["exchange_index"].get<int>());
    }
    // if the choice is only limited to 1 item, no need to show it
    if (list.size() == 1) { exchanges->setVisible(false); }
  }

  // create combo for 1st ticker selection
  std::vector<QComboBox*> qtickers;
  QComboBox* ticker1 = new QComboBox(widget);
  ticker1->setObjectName(fmt::format("Ticker_1"));
  layout->addWidget(ticker1);
  qtickers.push_back(ticker1);

  // create combo for 2nd ticker selection
  QComboBox* ticker2 = new QComboBox(widget);
  ticker2->setObjectName(fmt::format("Ticker_2"));
  layout->addWidget(ticker2);
  qtickers.push_back(ticker2);

  // callback triggered when exchange combo is modified
  // this lambda will set the ticker combo using the tickers available from the exchanges
  auto set_ticker_strings = [&](int /*index*/) {
    std::string text = exchanges->currentText().toStdString();
    auto ex =
        ranges::find_if(global_settings.networks_, [&](auto e) { return (e->get_name() == text); });
    if (ex == global_settings.networks_.end()) return;
    //
    auto tickers = (*ex)->tickers_subscribed();
    for (auto combo : qtickers)
    {
      QString currentText = combo->currentText();
      QStringList temp;
      for (auto const [cp, td] : tickers) { temp << currency_pair_qstring(cp); }
      combo->clear();
      combo->addItems(temp);
      // try to restore active selection if it is still there
      if (currentText != "") combo->setCurrentText(currentText);
    }
  };

  QWidget::connect(exchanges, &QComboBox::currentIndexChanged, widget,
      [=](int index) { set_ticker_strings(index); });

  // fill tickers with user supplied strings
  if (defaults.contains("tickers1"))
  {
    auto tickers = to_qstringlist(defaults["tickers1"].get<std::vector<std::string>>());
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker1->addItems(tickers);
    if (defaults.contains("tickers1_index"))
    {
      ticker1->setCurrentIndex(defaults["tickers1_index"].get<int>());
    }
  }

  if (defaults.contains("tickers2"))
  {
    auto tickers = to_qstringlist(defaults["tickers2"].get<std::vector<std::string>>());
    widget->setProperty("TSize", QVariant(static_cast<int>(tickers.size())));
    ticker2->addItems(tickers);
    if (defaults.contains("tickers2_index"))
    {
      ticker2->setCurrentIndex(defaults["tickers2_index"].get<int>());
    }
  }

  // trigger callback to initially setup combo items if user did not supply any
  if (!defaults.contains("tickers1") && !defaults.contains("tickers2")) set_ticker_strings(0);

  return widget;
}
